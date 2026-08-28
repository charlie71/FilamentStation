/**
 * @file
 * @brief Implements tasks::bambuTask(): LAN-Mode MQTT connection handling
 *        for Bambu Lab printers. The protocol (topics, payload shapes,
 *        credential scheme) is community-reverse-engineered and
 *        unverified against real hardware; see docs/bambu-protocol.md.
 *        Every on-wire detail is confined to services/BambuProtocol.{h,cpp}
 *        so this file only owns the MQTT/TLS transport and the
 *        per-printerId command/event plumbing required by the FreeRTOS
 *        task architecture.
 */
#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <array>
#include <cstdio>
#include <cstring>

#include "config/BambuConfig.h"
#include "models/BambuMaterialMapping.h"
#include "models/BambuPrinterConfig.h"
#include "models/PrinterState.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/BambuProtocol.h"
#include "services/Logger.h"
#include "services/PsramAlloc.h"

namespace filament_station::tasks {
namespace {

/// @brief Tracks an in-flight AssignTray until the printer's own telemetry
///        confirms it, or it times out.
// publish() succeeding only confirms the MQTT broker accepted the packet,
// not that the printer applied it (a rejected/unsigned command looks
// identical on the wire, see docs/bambu-protocol.md). An AssignTray is
// therefore tracked as pending until either a subsequent report's tray
// telemetry matches what was sent, or kBambuAssignConfirmTimeoutMs elapses.
struct PendingTrayAssignment {
  bool active = false;              ///< Whether a confirmation is currently being awaited.
  std::uint32_t requestId = 0;      ///< Correlation id to report back once confirmed/failed.
  std::uint8_t amsId = 0;           ///< Target AMS index (wire-encoded).
  std::uint8_t trayId = 0;          ///< Target tray index (wire-encoded).
  // No spoolId field here on purpose -- BambuTask only confirms that the
  // printer's own tray_type/tray_color telemetry now matches what was sent
  // (see checkPendingTrayAssignment() below); the Spoolman association
  // itself is tracked and persisted entirely by AppTask, see
  // models/TraySpoolCache.h.
  // Empty expectedTrayType means "confirm the slot becomes empty" (Reset).
  char expectedTrayType[16]{};      ///< Material telemetry must report before this is considered confirmed.
  char expectedColorHex[9]{};       ///< Color telemetry must report before this is considered confirmed.
  TickType_t startedAtTicks = 0;    ///< Tick count when this assignment was sent, for the timeout check.
  // Throttles BambuAssignProgress events to once per whole second of
  // remaining time instead of every ~200ms serviceConnections() tick; -1
  // never matches a real remaining-seconds value, so the first tick always
  // sends.
  std::int32_t lastReportedRemainingSeconds = -1;  ///< Last remaining-seconds value reported via BambuAssignProgress.
};

/// @brief One printer's MQTT connection, state, and in-flight assignment tracking.
struct PrinterConnection {
  bool inUse = false;               ///< Whether this slot is currently assigned to a printer.
  bool reportedConnected = false;   ///< Whether a BambuConnected event has been sent for the current session.
  models::BambuPrinterConfig config{};  ///< Connection configuration (host, credentials, ...).
  models::PrinterState state{};     ///< Accumulated printer/AMS/tray state from reports.
  WiFiClientSecure tlsClient;       ///< TLS transport for the MQTT connection.
  PubSubClient mqttClient{tlsClient};  ///< MQTT client bound to #tlsClient.
  char reportTopic[config::kBambuTopicCapacity]{};   ///< Subscribed topic the printer publishes status reports on.
  char requestTopic[config::kBambuTopicCapacity]{};  ///< Topic used to publish commands to the printer.
  char clientId[32]{};               ///< MQTT client id used for this connection.
  // "print.sequence_id" on the wire; community docs (OpenBambuAPI) call for
  // incrementing by 1 per command. Reset to 1 on every (re)connect, a fresh
  // MQTT session.
  std::uint32_t nextSequenceId = 1;  ///< Next "print.sequence_id" value to send.
  PendingTrayAssignment pending{};   ///< In-flight AssignTray confirmation state, if any.
  // See kBambuReconnectIntervalMs (Robustheit/Diagnose, TASKS.md 10.4): a
  // dropped MQTT session (printer reboot, LAN hiccup) was previously never
  // retried on its own -- only an explicit BambuCommandType::Connect
  // (AppTask's connectAllEnabledPrinters(), itself only fired at boot and on
  // WifiGotIp) ever reconnected. 0 allows an immediate first retry.
  TickType_t lastReconnectAttemptAt = 0;  ///< Tick count of the last automatic reconnect attempt.
};

using PrinterConnections =
    std::array<PrinterConnection, models::kMaximumPrinters>;  ///< Fixed-size pool of printer connection slots.

/// @brief Sends a printer-state AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param requestId Correlation id.
/// @param printerId Printer this event concerns.
/// @param state Printer state snapshot to report.
/// @param text Text payload.
/// @param value Numeric payload.
void publishBambuEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                       std::uint32_t requestId, rtos::PrinterId printerId,
                       const models::PrinterState& state, const char* text,
                       std::int32_t value = 0) {
  // static, PSRAM-backed (services/PsramAlloc.h): BambuTask processes
  // exactly one command/report at a time (single FreeRTOS consumer, never
  // re-entrant); this large (~3KB) AppEvent was a stack-local here, the
  // same latent bug pattern already fixed in AppTask/ScaleTask/NfcTask/
  // SpoolmanTask this session, just never triggered yet because this
  // function's own call depth stayed shallow enough until now.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "BambuTask.publishBambuEvent");
  *event = rtos::AppEvent{};
  event->type = type;
  event->requestId = requestId;
  event->value = value;
  event->printerId = printerId;
  event->printerState = state;
  std::snprintf(event->text, sizeof(event->text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Bambu,
            "Event enqueue failed queue=app_event event=%u printer_id=%u",
            static_cast<unsigned>(type), static_cast<unsigned>(printerId));
  }
}

/// @brief Sends a BambuError AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param printerId Printer this error concerns.
/// @param text Error text.
void publishBambuError(rtos::RtosContext& ctx, std::uint32_t requestId,
                       rtos::PrinterId printerId, const char* text) {
  publishBambuEvent(ctx, rtos::AppEventType::BambuError, requestId, printerId,
                    models::PrinterState{}, text);
}

/// @brief Publishes a request payload to a printer's request topic.
/// @param conn Connection to publish on.
/// @param printerId Printer id, for logging.
/// @param payload JSON payload to publish.
/// @return true if the MQTT broker accepted the packet (not proof the printer applied it).
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

/// @brief Finds the connection slot for a printer id, optionally allocating a free one.
/// @param connections Connection pool to search.
/// @param printerId Printer id to find.
/// @param allowCreate Whether to allocate and return a free slot if none is in use for `printerId`.
/// @return Pointer to the connection, or nullptr if not found and (no free slot or `allowCreate` is false).
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

/// @brief Checks whether a pending AssignTray's expected material/color now
///        matches the printer's own telemetry, and if so, confirms it to AppTask.
/// @param ctx Owning RTOS context.
/// @param conn Connection whose pending assignment to check.
// Compares a pending AssignTray's expected tray_type/tray_color against
// what the printer's own telemetry now reports for that slot; on a match,
// commits the Spoolman association and reports success to AppTask. Called
// after every successfully applied report -- multiple reports may pass
// before the printer's telemetry catches up (or never does, see the
// timeout check in serviceConnections()).
void checkPendingTrayAssignment(rtos::RtosContext& ctx,
                                PrinterConnection& conn) {
  if (!conn.pending.active) return;
  // Das externe/manuelle Fach (kein AMS) hat keinen Eintrag in
  // conn.state.amsUnits[] -- findSlot() findet es grundsaetzlich nie
  // (Nutzerbericht 2026-08-27: "Extern konfigurieren" bestaetigte deshalb
  // nie, auch nachdem der Drucker die Zuordnung laengst angenommen hatte).
  const models::PrinterSlotStateData* slot =
      conn.pending.amsId == models::kBambuExternalAmsId
          ? &conn.state.externalSlot
          : models::findSlot(conn.state, conn.pending.amsId,
                             conn.pending.trayId);
  if (slot == nullptr) return;
  const bool clearing = conn.pending.expectedTrayType[0] == '\0';
  const bool matches =
      clearing
          ? slot->material[0] == '\0'
          : std::strcmp(slot->material, conn.pending.expectedTrayType) == 0 &&
                std::strcmp(slot->colorHex, conn.pending.expectedColorHex) ==
                    0;
  if (!matches) return;

  const std::uint32_t requestId = conn.pending.requestId;
  const std::uint8_t amsId = conn.pending.amsId;
  const std::uint8_t trayId = conn.pending.trayId;
  conn.pending = {};
  FS_LOGI(services::LogComponent::Bambu,
          "AssignTray confirmed printer_id=%u ams_id=%u tray_id=%u",
          static_cast<unsigned>(conn.config.printerId),
          static_cast<unsigned>(amsId), static_cast<unsigned>(trayId));
  publishBambuEvent(ctx, rtos::AppEventType::BambuUpdate, requestId,
                    conn.config.printerId, conn.state,
                    "Slot vom Drucker best\xC3\xA4tigt");
}

/// @brief MQTT callback for a message on a printer's report topic: parses,
///        applies, and logs it, then checks any pending assignment.
/// @param ctx Owning RTOS context.
/// @param conn Connection the message arrived on.
/// @param payload Raw (not NUL-terminated) MQTT payload bytes.
/// @param length Length of `payload` in bytes.
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
  FS_LOGT(services::LogComponent::Bambu,
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
      FS_LOGT(services::LogComponent::Bambu,
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
  FS_LOGT(services::LogComponent::Bambu,
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
      FS_LOGT(services::LogComponent::Bambu,
              "Report tray detail printer_id=%u ams_id=%u tray_id=%u "
              "state=%u material=\"%s\" color=\"%s\"",
              static_cast<unsigned>(conn.config.printerId),
              static_cast<unsigned>(amsId), static_cast<unsigned>(trayId),
              static_cast<unsigned>(slot.state), slot.material,
              slot.colorHex);
    }
  }
  checkPendingTrayAssignment(ctx, conn);
  publishBambuEvent(ctx, rtos::AppEventType::BambuUpdate, 0,
                    conn.config.printerId, conn.state, "Statusbericht empfangen");
}

/// @brief Establishes the MQTT/TLS connection for a printer, subscribes,
///        and requests an initial full status report.
/// @param ctx Owning RTOS context.
/// @param conn Connection to establish; its `config` must already be set.
/// @param requestId Correlation id to report success/failure with.
/// @return true if the connection and subscription succeeded.
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
  // Defensive reset: every caller already clears this via disconnectPrinter()
  // before reaching here, but a fresh MQTT session has no pending
  // confirmation to track regardless.
  conn.pending = {};
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

/// @brief Fails and clears a connection's pending AssignTray, if any, reporting `reason` to AppTask.
/// @param ctx Owning RTOS context.
/// @param conn Connection whose pending assignment to fail.
/// @param reason Error text to report.
// A pending AssignTray confirmation left dangling (disconnect/reconnect
// before the printer's telemetry confirmed or timed it out) would strand
// AppTask's progress dialog on that requestId forever -- fail it explicitly
// instead of silently dropping it.
void failPendingAssignment(rtos::RtosContext& ctx, PrinterConnection& conn,
                           const char* reason) {
  if (!conn.pending.active) return;
  const std::uint32_t requestId = conn.pending.requestId;
  conn.pending = {};
  publishBambuError(ctx, requestId, conn.config.printerId, reason);
}

/// @brief Disconnects a printer's MQTT session and fails any pending assignment.
/// @param ctx Owning RTOS context.
/// @param conn Connection to disconnect.
void disconnectPrinter(rtos::RtosContext& ctx, PrinterConnection& conn) {
  if (conn.mqttClient.connected()) conn.mqttClient.disconnect();
  conn.reportedConnected = false;
  conn.state.connectionState = models::PrinterConnectionState::Offline;
  failPendingAssignment(ctx, conn, "Verbindung getrennt");
}

/// @brief Handles BambuCommandType::Connect: allocates/refreshes a connection and connects.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
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

/// @brief Handles BambuCommandType::Disconnect: disconnects and frees the connection slot.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
void handleDisconnect(rtos::RtosContext& ctx, PrinterConnections& connections,
                      const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Drucker ist nicht verbunden");
    return;
  }
  disconnectPrinter(ctx, *conn);
  conn->inUse = false;
  FS_LOGI(services::LogComponent::Bambu, "Disconnected printer_id=%u",
          static_cast<unsigned>(command.printerId));
  publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected,
                    command.requestId, command.printerId, conn->state,
                    "Drucker getrennt");
}

/// @brief Handles BambuCommandType::TestConnection: connects and immediately
///        disconnects an ephemeral connection to verify credentials.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
void handleTestConnection(rtos::RtosContext& ctx,
                          const rtos::BambuCommand& command) {
  // Ephemeral, not stored in the connection slots: verifies credentials
  // without disturbing an existing persistent connection for this printer.
  PrinterConnection probe{};
  probe.config = command.printerConfig;
  const bool ok = doConnect(ctx, probe, 0);
  if (ok) disconnectPrinter(ctx, probe);
  publishBambuEvent(
      ctx, rtos::AppEventType::BambuTestResult, command.requestId,
      command.printerId, models::PrinterState{},
      ok ? "Verbindungstest erfolgreich"
         : "Verbindungstest fehlgeschlagen",
      ok ? 1 : 0);
}

/// @brief Handles BambuCommandType::RequestStatus: publishes a pushall request.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
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

/// @brief Handles BambuCommandType::AssignTray: resolves the material to a
///        services::BambuMaterialMapping, sends
///        ams_filament_setting+extrusion_cali_sel, and starts tracking a
///        PendingTrayAssignment for confirmation.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
/// @note Rejects (BambuError, no MQTT traffic sent) if `command.trayType`
///       is non-empty but has no known services::BambuMaterialMapping
///       entry -- never guesses a profile for an unrecognized material.
void handleAssignTray(rtos::RtosContext& ctx, PrinterConnections& connections,
                      const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr || !conn->mqttClient.connected()) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Drucker ist nicht verbunden");
    return;
  }
  // Das externe/manuelle Fach kommt hier bereits wire-kodiert an
  // (models::kBambuExternalAmsId/kBambuExternalTrayId = 255/254, siehe
  // AppTask.cpp::sendPendingSlotAssignTray()) -- eine vierte, unabhaengige
  // Validierungsstelle, die bei der Fehlersuche zum externen Fach zuvor
  // uebersehen wurde (Nutzerbericht 2026-08-27: "Ungueltiger AMS-Slot"
  // bestand trotz dreier bereits behobener Stellen in AppTask.cpp weiter).
  const bool isExternalWireAddress =
      command.amsId == models::kBambuExternalAmsId &&
      command.trayId == models::kBambuExternalTrayId;
  if (!isExternalWireAddress &&
      (command.amsId >= models::kMaximumAmsPerPrinter ||
       command.trayId >= models::kSlotsPerAms)) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Ung\xC3\xBCltiger AMS-Slot");
    return;
  }

  // command.trayType is the raw Spoolman material name (empty clears the
  // slot, see rtos::BambuCommand::trayType) -- resolved here to Bambu's own
  // AMS profile (tray_info_idx/canonical tray_type/nozzle_temp_min/max).
  // Deliberately NOT the individual Spoolman filament's bambu_temp_min/
  // bambu_temp_max: those only ever affect the load/unload temperature
  // range shown on the AMS slot, not the actual print temperature (that
  // comes from the slicer/filament profile used for a print job), so
  // Bambu's own generic-material defaults are used instead -- see
  // services::resolveBambuMaterial() and docs/bambu-protocol.md.
  services::BambuTrayFilament filament{};
  if (command.trayType[0] != '\0') {
    // Material-mapping table is loaded at boot (and refreshed on a
    // successful download) by tasks::storageTask() -- BambuTask has no SD
    // access of its own, so it reads the already-validated, published
    // snapshot via this atomic pointer rather than the SD card directly
    // (see rtos/RtosContext.h, docs/bambu-protocol.md).
    const auto* table = ctx.bambuMaterialMappings.load(std::memory_order_acquire);
    if (table == nullptr) {
      FS_LOGW(services::LogComponent::Bambu,
              "AssignTray rejected printer_id=%u ams_id=%u tray_id=%u "
              "reason=material_mapping_unavailable",
              static_cast<unsigned>(command.printerId),
              static_cast<unsigned>(command.amsId),
              static_cast<unsigned>(command.trayId));
      publishBambuError(
          ctx, command.requestId, command.printerId,
          "Material-Zuordnung nicht verf\xC3\xBCgbar (bambu_materials.json fehlt oder ist ung\xC3\xBCltig)");
      return;
    }
    const models::BambuMaterialMappingEntry* mapping =
        services::resolveBambuMaterial(*table, command.trayType);
    if (mapping == nullptr) {
      // No invented/guessed profile for an unrecognized material -- reject
      // outright rather than silently sending a wrong or empty
      // tray_info_idx/temperature range.
      FS_LOGW(services::LogComponent::Bambu,
              "[BAMBU] Material resolve failed input=\"%s\" reason=no_mapping",
              command.trayType);
      FS_LOGW(services::LogComponent::Bambu,
              "AssignTray rejected printer_id=%u ams_id=%u tray_id=%u "
              "material=\"%s\" reason=no_material_mapping",
              static_cast<unsigned>(command.printerId),
              static_cast<unsigned>(command.amsId),
              static_cast<unsigned>(command.trayId), command.trayType);
      publishBambuError(ctx, command.requestId, command.printerId,
                        "Unbekanntes Material, keine Bambu-Zuordnung hinterlegt");
      return;
    }
    std::snprintf(filament.trayInfoIdx, sizeof(filament.trayInfoIdx), "%s",
                 mapping->trayInfoIdx);
    std::snprintf(filament.trayType, sizeof(filament.trayType), "%s",
                 mapping->trayType);
    filament.nozzleTempMinC = mapping->nozzleTempMinC;
    filament.nozzleTempMaxC = mapping->nozzleTempMaxC;
    FS_LOGD(services::LogComponent::Bambu,
            "[BAMBU] Material resolved input=\"%s\" canonical=\"%s\" "
            "tray_info_idx=\"%s\" tray_type=\"%s\" temp_min=%u temp_max=%u",
            command.trayType, mapping->material, mapping->trayInfoIdx,
            mapping->trayType, static_cast<unsigned>(mapping->nozzleTempMinC),
            static_cast<unsigned>(mapping->nozzleTempMaxC));
  }
  std::snprintf(filament.trayColorHex, sizeof(filament.trayColorHex), "%s",
               command.trayColorHex);
  FS_LOGD(services::LogComponent::Bambu,
          "AssignTray resolve printer_id=%u ams_id=%u tray_id=%u "
          "spool_id=%lu material=\"%s\" tray_info_idx=\"%s\" tray_type=\"%s\" "
          "color=\"%s\" temp_min=%u temp_max=%u temp_source=bambu_material_mapping",
          static_cast<unsigned>(command.printerId),
          static_cast<unsigned>(command.amsId),
          static_cast<unsigned>(command.trayId),
          static_cast<unsigned long>(command.spoolId), command.trayType,
          filament.trayInfoIdx, filament.trayType, filament.trayColorHex,
          static_cast<unsigned>(filament.nozzleTempMinC),
          static_cast<unsigned>(filament.nozzleTempMaxC));

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
  const char* nozzleDiameter =
      conn->state.nozzleDiameter[0] != '\0' ? conn->state.nozzleDiameter : "0.4";
  char caliSelPayload[config::kBambuRequestPayloadCapacity]{};
  const std::size_t caliSelLength = services::bambuBuildExtrusionCaliSel(
      conn->nextSequenceId++, command.amsId, command.trayId,
      filament.trayInfoIdx, nozzleDiameter, -1, caliSelPayload,
      sizeof(caliSelPayload));
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

  // publish() succeeding only means the MQTT broker accepted the packet --
  // not that the printer applied it (a rejected/unsigned command looks
  // identical on the wire, see docs/bambu-protocol.md). Report success to
  // AppTask only once the printer's own telemetry confirms the change
  // (checkPendingTrayAssignment(), called from handleReportPayload()), or
  // fail it after kBambuAssignConfirmTimeoutMs (serviceConnections()). A
  // still-active previous pending (shouldn't normally happen, AppTask
  // serializes slot assignments) is failed first rather than silently
  // dropped, so its requestId doesn't strand a progress dialog forever.
  failPendingAssignment(ctx, *conn,
                        "\xC3\x9C" "berholt durch neue Slot-Zuordnung");
  conn->pending.active = true;
  conn->pending.requestId = command.requestId;
  conn->pending.amsId = command.amsId;
  conn->pending.trayId = command.trayId;
  std::snprintf(conn->pending.expectedTrayType,
               sizeof(conn->pending.expectedTrayType), "%s",
               filament.trayType);
  services::bambuNormalizeTrayColorHex(filament.trayColorHex,
                                       conn->pending.expectedColorHex);
  conn->pending.startedAtTicks = xTaskGetTickCount();

  FS_LOGI(services::LogComponent::Bambu,
          "Tray filament setting sent, awaiting confirmation printer_id=%u "
          "ams_id=%u tray_id=%u tray_info_idx=\"%s\" tray_type=\"%s\" "
          "tray_color=\"%s\" nozzle_temp_min=%d nozzle_temp_max=%d",
          static_cast<unsigned>(command.printerId),
          static_cast<unsigned>(command.amsId),
          static_cast<unsigned>(command.trayId), filament.trayInfoIdx,
          filament.trayType, filament.trayColorHex,
          static_cast<int>(filament.nozzleTempMinC),
          static_cast<int>(filament.nozzleTempMaxC));
  // Raw wire payload already logged by publishBambuRequest() above.
}

/// @brief Handles BambuCommandType::Reset: disconnects and clears all state for a printer.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
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
  disconnectPrinter(ctx, *conn);
  conn->inUse = false;
  conn->state = models::PrinterState{};
  FS_LOGI(services::LogComponent::Bambu, "Reset printer_id=%u",
          static_cast<unsigned>(command.printerId));
  publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected,
                    command.requestId, command.printerId, conn->state,
                    "Zustand zur\xC3\xBC" "ckgesetzt");
}

/// @brief Handles BambuCommandType::Reconnect: forces a disconnect+reconnect cycle.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
void handleReconnect(rtos::RtosContext& ctx, PrinterConnections& connections,
                     const rtos::BambuCommand& command) {
  PrinterConnection* conn =
      connectionFor(connections, command.printerId, false);
  if (conn == nullptr) {
    publishBambuError(ctx, command.requestId, command.printerId,
                      "Kein bekannter Drucker; zuerst verbinden");
    return;
  }
  if (conn->mqttClient.connected()) disconnectPrinter(ctx, *conn);
  doConnect(ctx, *conn, command.requestId);
}

/// @brief Validates the printer id and dispatches a BambuCommand to its handler.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
/// @param command Command to process.
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

/// @brief Per-loop maintenance for every connection: services the MQTT
///        client, checks pending-assignment timeouts/progress, detects
///        connection loss, and drives the reconnect backoff.
/// @param ctx Owning RTOS context.
/// @param connections Connection pool.
void serviceConnections(rtos::RtosContext& ctx, PrinterConnections& connections) {
  for (auto& conn : connections) {
    if (!conn.inUse) continue;
    if (conn.mqttClient.connected()) {
      conn.mqttClient.loop();
      if (conn.pending.active) {
        const std::uint32_t elapsedMs = pdTICKS_TO_MS(static_cast<TickType_t>(
            xTaskGetTickCount() - conn.pending.startedAtTicks));
        if (elapsedMs >= config::kBambuAssignConfirmTimeoutMs) {
          FS_LOGW(services::LogComponent::Bambu,
                  "AssignTray confirmation timeout printer_id=%u ams_id=%u "
                  "tray_id=%u",
                  static_cast<unsigned>(conn.config.printerId),
                  static_cast<unsigned>(conn.pending.amsId),
                  static_cast<unsigned>(conn.pending.trayId));
          failPendingAssignment(
              ctx, conn,
              "Drucker hat die Slot\xC3\xA4nderung nicht best\xC3\xA4tigt");
        } else {
          // Progress feedback for the UI's wait dialog (see AppTask's
          // BambuAssignProgress handling): remaining time until the
          // timeout above fires, throttled to once per whole second so
          // this doesn't flood the AppEvent queue every 200ms tick.
          const std::uint32_t remainingMs =
              config::kBambuAssignConfirmTimeoutMs - elapsedMs;
          const std::int32_t remainingSeconds =
              static_cast<std::int32_t>((remainingMs + 999) / 1000);
          if (remainingSeconds != conn.pending.lastReportedRemainingSeconds) {
            conn.pending.lastReportedRemainingSeconds = remainingSeconds;
            publishBambuEvent(ctx, rtos::AppEventType::BambuAssignProgress,
                              conn.pending.requestId, conn.config.printerId,
                              conn.state, "",
                              static_cast<std::int32_t>(remainingMs));
          }
        }
      }
      continue;
    }
    if (conn.reportedConnected) {
      conn.reportedConnected = false;
      conn.state.connectionState = models::PrinterConnectionState::Offline;
      FS_LOGW(services::LogComponent::Bambu, "Connection lost printer_id=%u",
              static_cast<unsigned>(conn.config.printerId));
      failPendingAssignment(ctx, conn, "Verbindung verloren");
      publishBambuEvent(ctx, rtos::AppEventType::BambuDisconnected, 0,
                        conn.config.printerId, conn.state,
                        "Verbindung verloren");
    }
    if (!conn.config.enabled) continue;
    const TickType_t now = xTaskGetTickCount();
    if (static_cast<TickType_t>(now - conn.lastReconnectAttemptAt) <
        pdMS_TO_TICKS(config::kBambuReconnectBackoffMs)) {
      continue;
    }
    conn.lastReconnectAttemptAt = now;
    FS_LOGI(services::LogComponent::Bambu,
            "Attempting MQTT reconnect printer_id=%u host=\"%s\"",
            static_cast<unsigned>(conn.config.printerId), conn.config.host);
    doConnect(ctx, conn, 0);
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
