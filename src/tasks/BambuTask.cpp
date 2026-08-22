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
  // "print.sequence_id" on the wire; community docs (OpenBambuAPI) call for
  // incrementing by 1 per command. Reset to 1 on every (re)connect, a fresh
  // MQTT session.
  std::uint32_t nextSequenceId = 1;
};

using PrinterConnections =
    std::array<PrinterConnection, models::kMaximumPrinters>;

void publishBambuEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                       std::uint32_t requestId, rtos::PrinterId printerId,
                       const models::PrinterState& state, const char* text,
                       std::int32_t value = 0) {
  // static: BambuTask processes exactly one command/report at a time
  // (single FreeRTOS consumer, never re-entrant); this large (~3KB)
  // AppEvent was a stack-local here, the same latent bug pattern already
  // fixed in AppTask/ScaleTask/NfcTask/SpoolmanTask this session, just
  // never triggered yet because this function's own call depth stayed
  // shallow enough until now.
  static rtos::AppEvent event{};
  event = rtos::AppEvent{};
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

// Single choke point for every outgoing MQTT publish so the exact bytes
// sent to the printer are always visible in the log, not just for the one
// command that happened to have ad-hoc logging added -- own log line with
// minimal prefix (see kLogMessageCapacity, TaskConfig.h) so the payload is
// never truncated.
bool publishBambuRequest(PrinterConnection& conn, rtos::PrinterId printerId,
                         const char* payload) {
  FS_LOGD(services::LogComponent::Bambu,
          "MQTT publish printer_id=%u topic=\"%s\" payload=%s",
          static_cast<unsigned>(printerId), conn.requestTopic, payload);
  const bool published = conn.mqttClient.publish(conn.requestTopic, payload);
  if (!published) {
    FS_LOGW(services::LogComponent::Bambu,
            "MQTT publish failed printer_id=%u topic=\"%s\"",
            static_cast<unsigned>(printerId), conn.requestTopic);
  }
  return published;
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
  // Own log line (own kLogMessageCapacity-bounded line, same reasoning as
  // publishBambuRequest()) so a command rejection is visible even when
  // bambuApplyReport() below has no field for it -- payload is not
  // null-terminated, hence "%.*s" with the explicit length.
  FS_LOGD(services::LogComponent::Bambu,
          "Report raw payload printer_id=%u payload=%.*s",
          static_cast<unsigned>(conn.config.printerId),
          static_cast<int>(length), reinterpret_cast<const char*>(payload));

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

  if (document["print"].is<JsonObjectConst>()) {
    // Command replies (e.g. after ams_filament_setting/extrusion_cali_sel)
    // carry "result"/"reason"/"err_code" on rejection -- the printer's own
    // MQTT command verification (Developer Mode requirement) rejects
    // unsigned commands exactly this way. bambuApplyReport() below never
    // looks at these fields, so a rejection was previously invisible; only
    // the (unchanged) tray telemetry hinted at it afterwards.
    const JsonObjectConst print = document["print"].as<JsonObjectConst>();
    const char* command = print["command"] | "";
    if (command[0] != '\0') {
      const char* result = print["result"] | "";
      const char* reason = print["reason"] | "";
      const long errCode = print["err_code"] | 0L;
      FS_LOGI(services::LogComponent::Bambu,
              "MQTT command reply printer_id=%u command=\"%s\" result=\"%s\" "
              "reason=\"%s\" err_code=%ld",
              static_cast<unsigned>(conn.config.printerId), command, result,
              reason, errCode);
    }
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
  // Per-tray detail (diagnostic): confirms whether a just-sent
  // ams_filament_setting actually changed what the printer reports back,
  // versus the app only assuming success from the MQTT publish result.
  for (std::uint8_t amsId = 0; amsId < models::kMaximumAmsPerPrinter;
       ++amsId) {
    const auto& ams = conn.state.amsUnits[amsId];
    if (!ams.present) continue;
    for (std::uint8_t trayId = 0; trayId < models::kSlotsPerAms; ++trayId) {
      const auto& slot = ams.slots[trayId];
      FS_LOGD(services::LogComponent::Bambu,
              "Report tray detail printer_id=%u ams_id=%u tray_id=%u "
              "state=%u material=\"%s\" color=\"%s\" spool_id=%lu",
              static_cast<unsigned>(conn.config.printerId),
              static_cast<unsigned>(amsId), static_cast<unsigned>(trayId),
              static_cast<unsigned>(slot.state), slot.material,
              slot.colorHex, static_cast<unsigned long>(slot.spoolId));
    }
  }
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
  conn.nextSequenceId = 1;
  char request[config::kBambuRequestPayloadCapacity]{};
  if (services::bambuBuildPushAllRequest(conn.nextSequenceId++, request,
                                         sizeof(request)) > 0) {
    publishBambuRequest(conn, conn.config.printerId, request);
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
    if (services::bambuBuildPushAllRequest(conn->nextSequenceId++, request,
                                           sizeof(request)) > 0) {
      publishBambuRequest(*conn, conn->config.printerId, request);
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
  if (services::bambuBuildPushAllRequest(conn->nextSequenceId++, request,
                                         sizeof(request)) == 0 ||
      !publishBambuRequest(*conn, command.printerId, request)) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Statusanfrage konnte nicht gesendet werden");
    return;
  }
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
      conn->nextSequenceId++, command.amsId, command.trayId, filament,
      payload, sizeof(payload));
  if (length == 0 || !publishBambuRequest(*conn, command.printerId, payload)) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Slotdaten konnten nicht gesendet werden");
    return;
  }

  // "ams_filament_setting" alone leaves the printer treating the change as
  // provisional; a follow-up "extrusion_cali_sel" is what actually commits
  // it (see the doc comment on bambuBuildExtrusionCaliSel() and
  // docs/bambu-protocol.md). caliIdx -1: no specific flow/pressure-advance
  // calibration is tracked per Spoolman spool yet.
  const char* trayInfoIdx = services::bambuGenericTrayInfoIdx(filament.trayType);
  const char* nozzleDiameter =
      conn->state.nozzleDiameter[0] != '\0' ? conn->state.nozzleDiameter : "0.4";
  char caliSelPayload[config::kBambuRequestPayloadCapacity]{};
  const std::size_t caliSelLength = services::bambuBuildExtrusionCaliSel(
      conn->nextSequenceId++, command.amsId, command.trayId, trayInfoIdx,
      nozzleDiameter, -1, caliSelPayload, sizeof(caliSelPayload));
  if (caliSelLength == 0 ||
      !publishBambuRequest(*conn, command.printerId, caliSelPayload)) {
    FS_LOGW(services::LogComponent::Bambu,
            "extrusion_cali_sel send failed printer_id=%u ams_id=%u "
            "tray_id=%u -- Zuordnung wird m\xC3\xB6glicherweise nicht "
            "dauerhaft \xC3\xBC" "bernommen",
            static_cast<unsigned>(command.printerId),
            static_cast<unsigned>(command.amsId),
            static_cast<unsigned>(command.trayId));
  }

  // The printer itself does not know Spoolman IDs; this is the one place
  // BambuTask records the application-side association, distinct from
  // bambuApplyReport() which never touches spoolId (see
  // docs/bambu-protocol.md).
  conn->state.amsUnits[command.amsId].slots[command.trayId].spoolId =
      command.spoolId;
  FS_LOGI(services::LogComponent::Bambu,
          "Tray filament setting sent printer_id=%u ams_id=%u tray_id=%u "
          "tray_info_idx=\"%s\" tray_type=\"%s\" tray_color=\"%s\" "
          "nozzle_temp_min=%d nozzle_temp_max=%d",
          static_cast<unsigned>(command.printerId),
          static_cast<unsigned>(command.amsId),
          static_cast<unsigned>(command.trayId),
          services::bambuGenericTrayInfoIdx(filament.trayType),
          filament.trayType, filament.trayColorHex,
          static_cast<int>(filament.nozzleTempMinC),
          static_cast<int>(filament.nozzleTempMaxC));
  // Raw wire payload already logged by publishBambuRequest() above.
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
