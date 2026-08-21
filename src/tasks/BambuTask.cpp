#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <array>
#include <cstdio>
#include <cstring>

#include "config/BambuConfig.h"
#include "models/BambuPrinterConfig.h"
#include "models/PrinterState.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/BambuProtocol.h"
#include "services/Logger.h"

// LAN-Mode MQTT connection handling for Bambu Lab printers. The protocol
// (topics, payload shapes, credential scheme) is community-reverse-engineered
// and unverified against real hardware; see docs/bambu-protocol.md. Every
// on-wire detail is confined to services/BambuProtocol.{h,cpp} so this file
// only owns the MQTT/TLS transport and the per-printerId command/event
// plumbing required by the FreeRTOS task architecture.

namespace filament_station::tasks {
namespace {

struct PrinterConnection {
  bool inUse = false;
  bool reportedConnected = false;
  models::BambuPrinterConfig config{};
  models::PrinterState state{};
  WiFiClientSecure tlsClient;
  PubSubClient mqttClient{tlsClient};
  char reportTopic[config::kBambuTopicCapacity]{};
  char requestTopic[config::kBambuTopicCapacity]{};
  char clientId[32]{};
};

using PrinterConnections =
    std::array<PrinterConnection, models::kMaximumPrinters>;

void publishBambuEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                       std::uint32_t requestId, rtos::PrinterId printerId,
                       const models::PrinterState& state, const char* text,
                       std::int32_t value = 0) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  event.value = value;
  event.printerId = printerId;
  event.printerState = state;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Bambu,
            "Event enqueue failed queue=app_event event=%u printer_id=%u",
            static_cast<unsigned>(type), static_cast<unsigned>(printerId));
  }
}

void publishBambuError(rtos::RtosContext& ctx, std::uint32_t requestId,
                       rtos::PrinterId printerId, const char* text) {
  publishBambuEvent(ctx, rtos::AppEventType::BambuError, requestId, printerId,
                    models::PrinterState{}, text);
}

PrinterConnection* connectionFor(PrinterConnections& connections,
                                 rtos::PrinterId printerId, bool allowCreate) {
  for (auto& conn : connections) {
    if (conn.inUse && conn.config.printerId == printerId) return &conn;
  }
  if (!allowCreate) return nullptr;
  for (auto& conn : connections) {
    if (!conn.inUse) return &conn;
  }
  return nullptr;
}

void handleReportPayload(rtos::RtosContext& ctx, PrinterConnection& conn,
                         const std::uint8_t* payload, unsigned int length) {
  FS_LOGT(services::LogComponent::Bambu,
          "Report message received printer_id=%u topic=\"%s\" bytes=%u",
          static_cast<unsigned>(conn.config.printerId), conn.reportTopic,
          length);
  JsonDocument document;
  const DeserializationError parseError =
      deserializeJson(document, payload, length);
  if (parseError) {
    FS_LOGW(services::LogComponent::Bambu,
            "Report parse failed printer_id=%u bytes=%u error=\"%s\"",
            static_cast<unsigned>(conn.config.printerId), length,
            parseError.c_str());
    return;
  }
  if (!services::bambuApplyReport(document, conn.state)) {
    // Message on the report topic without a "print" object: a different,
    // currently unhandled message type. Not an error (see
    // docs/bambu-protocol.md).
    FS_LOGD(services::LogComponent::Bambu,
            "Report message ignored printer_id=%u reason=no_print_object",
            static_cast<unsigned>(conn.config.printerId));
    return;
  }
  std::uint8_t presentAmsCount = 0;
  std::uint8_t occupiedTrayCount = 0;
  for (const auto& ams : conn.state.amsUnits) {
    if (!ams.present) continue;
    ++presentAmsCount;
    for (const auto& slot : ams.slots)
      if (slot.state == models::PrinterSlotState::Ready) ++occupiedTrayCount;
  }
  FS_LOGD(services::LogComponent::Bambu,
          "Report applied printer_id=%u ams_reported=%u ams_present=%u "
          "trays_occupied=%u external_state=%u",
          static_cast<unsigned>(conn.config.printerId),
          static_cast<unsigned>(conn.state.amsCount),
          static_cast<unsigned>(presentAmsCount),
          static_cast<unsigned>(occupiedTrayCount),
          static_cast<unsigned>(conn.state.externalSlot.state));
  publishBambuEvent(ctx, rtos::AppEventType::BambuUpdate, 0,
                    conn.config.printerId, conn.state, "Statusbericht empfangen");
}

bool doConnect(rtos::RtosContext& ctx, PrinterConnection& conn,
              std::uint32_t requestId) {
  std::snprintf(conn.clientId, sizeof(conn.clientId), "FS-%06lX-%u",
               static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFUL),
               static_cast<unsigned>(conn.config.printerId));
  services::bambuReportTopic(conn.config.serialNumber, conn.reportTopic,
                             sizeof(conn.reportTopic));
  services::bambuRequestTopic(conn.config.serialNumber, conn.requestTopic,
                              sizeof(conn.requestTopic));

  conn.tlsClient.setTimeout(config::kBambuConnectTimeoutMs);
  // LAN-Mode printers use a self-signed certificate with no public trust
  // chain; the LAN access code is the actual shared secret here (see
  // docs/bambu-protocol.md).
  conn.tlsClient.setInsecure();
  conn.mqttClient.setServer(conn.config.host, config::kBambuMqttPort);

  if (!conn.mqttClient.connect(conn.clientId, config::kBambuMqttUsername,
                               conn.config.accessCode)) {
    char text[96];
    std::snprintf(text, sizeof(text),
                  "MQTT-Verbindung fehlgeschlagen (Status %d)",
                  conn.mqttClient.state());
    FS_LOGW(services::LogComponent::Bambu,
            "Connect failed printer_id=%u host=\"%s\" state=%d",
            static_cast<unsigned>(conn.config.printerId), conn.config.host,
            conn.mqttClient.state());
    conn.state.connectionState = models::PrinterConnectionState::Error;
    publishBambuEvent(ctx, rtos::AppEventType::BambuError, requestId,
                      conn.config.printerId, conn.state, text);
    return false;
  }

  if (!conn.mqttClient.subscribe(conn.reportTopic)) {
    FS_LOGW(services::LogComponent::Bambu,
            "Report subscription failed printer_id=%u topic=\"%s\"",
            static_cast<unsigned>(conn.config.printerId), conn.reportTopic);
  }
  char request[config::kBambuRequestPayloadCapacity]{};
  if (services::bambuBuildPushAllRequest(request, sizeof(request)) > 0) {
    const bool published = conn.mqttClient.publish(conn.requestTopic, request);
    FS_LOGD(services::LogComponent::Bambu,
            "Initial pushall request %s printer_id=%u topic=\"%s\"",
            published ? "sent" : "failed",
            static_cast<unsigned>(conn.config.printerId), conn.requestTopic);
  }

  conn.state.printerId = conn.config.printerId;
  std::snprintf(conn.state.name, sizeof(conn.state.name), "%s",
               conn.config.name);
  conn.state.enabled = conn.config.enabled;
  conn.state.connectionState = models::PrinterConnectionState::Connected;
  conn.reportedConnected = true;
  FS_LOGI(services::LogComponent::Bambu,
          "Connected printer_id=%u host=\"%s\"",
          static_cast<unsigned>(conn.config.printerId), conn.config.host);
  publishBambuEvent(ctx, rtos::AppEventType::BambuConnected, requestId,
                    conn.config.printerId, conn.state, "Drucker verbunden");
  return true;
}

void disconnectPrinter(PrinterConnection& conn) {
  if (conn.mqttClient.connected()) conn.mqttClient.disconnect();
  conn.reportedConnected = false;
  conn.state.connectionState = models::PrinterConnectionState::Offline;
}

void handleConnect(rtos::RtosContext& ctx, PrinterConnections& connections,
                   const rtos::BambuCommand& command) {
  PrinterConnection* conn = connectionFor(connections, command.printerId, true);
  if (conn == nullptr) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Maximale Anzahl gleichzeitiger Druckerverbindungen erreicht");
    return;
  }
  conn->inUse = true;
  conn->config = command.printerConfig;
  if (conn->mqttClient.connected()) {
    // Already connected to this printerId: idempotent refresh instead of a
    // full reconnect (mirrors the AssignTag idempotency pattern).
    char request[config::kBambuRequestPayloadCapacity]{};
    if (services::bambuBuildPushAllRequest(request, sizeof(request)) > 0) {
      conn->mqttClient.publish(conn->requestTopic, request);
    }
    publishBambuEvent(ctx, rtos::AppEventType::BambuConnected,
                      command.requestId, conn->config.printerId, conn->state,
                      "Drucker bereits verbunden");
    return;
  }
  doConnect(ctx, *conn, command.requestId);
}

void handleDisconnect(rtos::RtosContext& ctx, PrinterConnections& connections,
                      const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Drucker ist nicht verbunden");
    return;
  }
  disconnectPrinter(*conn);
  conn->inUse = false;
  FS_LOGI(services::LogComponent::Bambu, "Disconnected printer_id=%u",
          static_cast<unsigned>(command.printerId));
  publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected,
                    command.requestId, command.printerId, conn->state,
                    "Drucker getrennt");
}

void handleTestConnection(rtos::RtosContext& ctx,
                          const rtos::BambuCommand& command) {
  // Ephemeral, not stored in the connection slots: verifies credentials
  // without disturbing an existing persistent connection for this printer.
  PrinterConnection probe{};
  probe.config = command.printerConfig;
  const bool ok = doConnect(ctx, probe, 0);
  if (ok) disconnectPrinter(probe);
  publishBambuEvent(
      ctx, rtos::AppEventType::BambuTestResult, command.requestId,
      command.printerId, models::PrinterState{},
      ok ? "Verbindungstest erfolgreich"
         : "Verbindungstest fehlgeschlagen",
      ok ? 1 : 0);
}

void handleRequestStatus(rtos::RtosContext& ctx,
                         PrinterConnections& connections,
                         const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr || !conn->mqttClient.connected()) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Drucker ist nicht verbunden");
    return;
  }
  char request[config::kBambuRequestPayloadCapacity]{};
  if (services::bambuBuildPushAllRequest(request, sizeof(request)) == 0 ||
      !conn->mqttClient.publish(conn->requestTopic, request)) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Statusanfrage konnte nicht gesendet werden");
    return;
  }
  FS_LOGD(services::LogComponent::Bambu,
          "Status request sent printer_id=%u topic=\"%s\"",
          static_cast<unsigned>(command.printerId), conn->requestTopic);
}

void handleAssignTray(rtos::RtosContext& ctx, PrinterConnections& connections,
                      const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr || !conn->mqttClient.connected()) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Drucker ist nicht verbunden");
    return;
  }
  if (command.amsId >= models::kMaximumAmsPerPrinter ||
      command.trayId >= models::kSlotsPerAms) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Ung\xC3\xBCltiger AMS-Slot");
    return;
  }

  services::BambuTrayFilament filament{};
  std::snprintf(filament.trayType, sizeof(filament.trayType), "%s",
               command.trayType);
  std::snprintf(filament.trayColorHex, sizeof(filament.trayColorHex), "%s",
               command.trayColorHex);
  filament.nozzleTempMinC = command.nozzleTempMinC;
  filament.nozzleTempMaxC = command.nozzleTempMaxC;

  char payload[config::kBambuRequestPayloadCapacity]{};
  const std::size_t length = services::bambuBuildAmsFilamentSetting(
      command.amsId, command.trayId, filament, payload, sizeof(payload));
  if (length == 0 || !conn->mqttClient.publish(conn->requestTopic, payload)) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Slotdaten konnten nicht gesendet werden");
    return;
  }

  // The printer itself does not know Spoolman IDs; this is the one place
  // BambuTask records the application-side association, distinct from
  // bambuApplyReport() which never touches spoolId (see
  // docs/bambu-protocol.md).
  conn->state.amsUnits[command.amsId].slots[command.trayId].spoolId =
      command.spoolId;
  FS_LOGI(services::LogComponent::Bambu,
          "Tray filament setting sent printer_id=%u ams_id=%u tray_id=%u",
          static_cast<unsigned>(command.printerId),
          static_cast<unsigned>(command.amsId),
          static_cast<unsigned>(command.trayId));
  publishBambuEvent(ctx, rtos::AppEventType::BambuUpdate, command.requestId,
                    command.printerId, conn->state, "Slotdaten gesendet");
}

void handleReset(rtos::RtosContext& ctx, PrinterConnections& connections,
                 const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr) {
    // Nothing to reset: idempotent success.
    publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected,
                      command.requestId, command.printerId,
                      models::PrinterState{}, "Zustand zur\xC3\xBC" "ckgesetzt");
    return;
  }
  disconnectPrinter(*conn);
  conn->inUse = false;
  conn->state = models::PrinterState{};
  FS_LOGI(services::LogComponent::Bambu, "Reset printer_id=%u",
          static_cast<unsigned>(command.printerId));
  publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected,
                    command.requestId, command.printerId, conn->state,
                    "Zustand zur\xC3\xBC" "ckgesetzt");
}

void handleReconnect(rtos::RtosContext& ctx, PrinterConnections& connections,
                     const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Kein bekannter Drucker; zuerst verbinden");
    return;
  }
  if (conn->mqttClient.connected()) disconnectPrinter(*conn);
  doConnect(ctx, *conn, command.requestId);
}

void handleCommand(rtos::RtosContext& ctx, PrinterConnections& connections,
                   const rtos::BambuCommand& command) {
  if (!models::isValidPrinterId(command.printerId)) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Ung\xC3\xBCltige Drucker-ID");
    return;
  }
  switch (command.type) {
    case rtos::BambuCommandType::Connect:
      handleConnect(ctx, connections, command);
      return;
    case rtos::BambuCommandType::Disconnect:
      handleDisconnect(ctx, connections, command);
      return;
    case rtos::BambuCommandType::TestConnection:
      handleTestConnection(ctx, command);
      return;
    case rtos::BambuCommandType::RequestStatus:
      handleRequestStatus(ctx, connections, command);
      return;
    case rtos::BambuCommandType::AssignTray:
      handleAssignTray(ctx, connections, command);
      return;
    case rtos::BambuCommandType::Reset:
      handleReset(ctx, connections, command);
      return;
    case rtos::BambuCommandType::Reconnect:
      handleReconnect(ctx, connections, command);
      return;
  }
}

void serviceConnections(rtos::RtosContext& ctx, PrinterConnections& connections) {
  for (auto& conn : connections) {
    if (!conn.inUse) continue;
    if (conn.mqttClient.connected()) {
      conn.mqttClient.loop();
      continue;
    }
    if (conn.reportedConnected) {
      conn.reportedConnected = false;
      conn.state.connectionState = models::PrinterConnectionState::Offline;
      FS_LOGW(services::LogComponent::Bambu, "Connection lost printer_id=%u",
              static_cast<unsigned>(conn.config.printerId));
      publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected, 0,
                        conn.config.printerId, conn.state,
                        "Verbindung verloren");
    }
  }
}

}  // namespace

void bambuTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  static PrinterConnections connections{};
  for (auto& conn : connections) {
    conn.mqttClient.setCallback(
        [&ctx, &conn](char* topic, std::uint8_t* payload, unsigned int length) {
          (void)topic;
          handleReportPayload(ctx, conn, payload, length);
        });
  }

  rtos::BambuCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.bambuCommandQueue, &command,
                      pdMS_TO_TICKS(config::kBambuServiceIntervalMs)) ==
        pdTRUE) {
      handleCommand(ctx, connections, command);
    }
    serviceConnections(ctx, connections);
  }
}

}  // namespace filament_station::tasks
