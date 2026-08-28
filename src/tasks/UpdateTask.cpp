/**
 * @file
 * @brief Implements tasks::updateTask(): GitHub-releases firmware-update
 *        check, download, SHA-256 verification, and flashing via the ESP32
 *        OTA (Update) library.
 */
#include "tasks/Tasks.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include <array>
#include <cstdio>
#include <cstring>

#include "config/AppConfig.h"
#include "config/BambuMaterialConfig.h"
#include "config/UpdateConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"
#include "services/PsramAlloc.h"
#include "services/SemVer.h"
#include "services/Sha256Hex.h"

namespace filament_station::tasks {
namespace {

/// @brief Sends a simple numeric/text AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param requestId Correlation id.
/// @param value Numeric payload.
/// @param text Text payload.
void sendEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
              std::uint32_t requestId, std::int32_t value, const char* text) {
  // static, PSRAM-backed (services/PsramAlloc.h): siehe SpoolmanTask::
  // sendResult() fuer die Begruendung (AppEvent ist gross, dieser Task
  // sendet immer nur ein Ergebnis/einen Fortschrittsschritt zur Zeit).
  static rtos::AppEvent* event = services::allocatePsramInstance<rtos::AppEvent>(
      "UpdateTask.sendEvent");
  *event = rtos::AppEvent{};
  event->type = type;
  event->requestId = requestId;
  event->value = value;
  std::snprintf(event->text, sizeof(event->text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Update,
            "Event enqueue failed queue=app_event type=%u",
            static_cast<unsigned>(type));
  }
}

/// @brief Sends a StorageCommand to tasks::storageTask(), fire-and-forget --
///        the eventual result (success or failure) arrives independently on
///        appEventQueue, which only tasks::appTask() consumes; this task
///        never waits for it directly (see docs/architecture.md).
/// @param ctx Owning RTOS context.
/// @param command Command to send.
void sendStorageCommand(rtos::RtosContext& ctx,
                        const rtos::StorageCommand& command) {
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Update,
            "Command enqueue failed queue=storage_command type=%u",
            static_cast<unsigned>(command.type));
  }
}

/// @brief Fetches a plain-text checksum file and extracts its SHA-256 hex digest.
/// @param url URL of the ".sha256" checksum file.
/// @param outHex64 Destination buffer, at least 65 bytes.
/// @return false on any request failure or invalid checksum content.
// Kleine reine Textantwort (kein JSON) -- eigene, einfachere Anfrage statt
// getJson()-artiger Hilfsfunktion, da hier nur eine Zeile erwartet wird.
bool fetchChecksum(const char* url, char* outHex64) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(config::kUpdateCheckTimeoutMs);
  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(config::kUpdateCheckTimeoutMs));
  http.setTimeout(static_cast<uint16_t>(config::kUpdateCheckTimeoutMs));
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  http.addHeader("User-Agent", config::kUpdateUserAgent);
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    FS_LOGW(services::LogComponent::Update,
            "Checksum request failed url=\"%s\" status=%d", url, status);
    http.end();
    return false;
  }
  const String body = http.getString();
  http.end();
  return services::extractHexSha256(body.c_str(), outHex64);
}

/// @brief Handles UpdateCommandType::CheckForUpdate: queries the GitHub
///        latest-release tag and compares it against the running firmware version.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
void checkForUpdate(rtos::RtosContext& ctx, std::uint32_t requestId) {
  char url[128];
  std::snprintf(url, sizeof(url), "https://%s/repos/%s/%s/releases/latest",
                config::kUpdateApiHost, config::kUpdateRepoOwner,
                config::kUpdateRepoName);

  // LAN-freie, oeffentliche API -- setInsecure() wie bereits bei BambuTasks
  // WiFiClientSecure etabliert (kein Security-Key/keine Zertifikatspflege
  // geplant, siehe TASKS.md Phase 13.1/14.8).
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(config::kUpdateCheckTimeoutMs);

  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(config::kUpdateCheckTimeoutMs));
  http.setTimeout(static_cast<uint16_t>(config::kUpdateCheckTimeoutMs));
  if (!http.begin(client, url)) {
    FS_LOGE(services::LogComponent::Update,
            "Request failed method=GET url=\"%s\" error=\"begin failed\"", url);
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, -1, "Verbindung zur Update-Quelle fehlgeschlagen");
    return;
  }
  // GitHub lehnt Anfragen ohne User-Agent-Header ab.
  http.addHeader("User-Agent", config::kUpdateUserAgent);

  const int status = http.GET();
  if (status == 404) {
    http.end();
    FS_LOGI(services::LogComponent::Update,
            "No releases published yet url=\"%s\"", url);
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, -1, "Keine Version ver\xC3\xB6" "ffentlicht");
    return;
  }
  if (status != HTTP_CODE_OK) {
    char error[64];
    std::snprintf(error, sizeof(error), "HTTP-Anfrage fehlgeschlagen (%d)", status);
    FS_LOGW(services::LogComponent::Update,
            "Request failed method=GET url=\"%s\" status=%d", url, status);
    http.end();
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, -1, error);
    return;
  }

  // Nur "tag_name" behalten -- die volle Release-Antwort (Beschreibung,
  // Assets-Liste, ...) ist mehrere KB gross und wird nicht gebraucht.
  JsonDocument filter;
  filter["tag_name"] = true;
  JsonDocument document;
  const DeserializationError jsonError = deserializeJson(
      document, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jsonError || !document["tag_name"].is<const char*>()) {
    FS_LOGE(services::LogComponent::Update,
            "Request failed method=GET url=\"%s\" error=\"invalid response\" "
            "json_error=\"%s\"",
            url, jsonError.c_str());
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, -1, "Ung\xC3\xBCltige Serverantwort");
    return;
  }

  const char* tagName = document["tag_name"];
  services::SemVer latest{};
  services::SemVer current{};
  if (!services::parseSemVer(tagName, latest) ||
      !services::parseSemVer(config::kApplicationVersion, current)) {
    FS_LOGW(services::LogComponent::Update,
            "Version not comparable tag=\"%s\" current=\"%s\"", tagName,
            config::kApplicationVersion);
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, -1, "Version nicht auswertbar");
    return;
  }

  if (services::compareSemVer(latest, current) > 0) {
    FS_LOGI(services::LogComponent::Update, "Update available tag=\"%s\"",
            tagName);
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, 1, tagName);
  } else {
    FS_LOGI(services::LogComponent::Update, "Firmware up to date tag=\"%s\"",
            tagName);
    sendEvent(ctx, rtos::AppEventType::UpdateCheckResult, requestId, 0, "");
  }
}

/// @brief Downloads, SHA-256-verifies, and flashes the firmware image via
///        the ESP32 OTA (Update) library. Sends UpdateDownloadProgress
///        events while streaming; does *not* send the final
///        UpdateDownloadResult itself -- the caller (downloadUpdate()) does
///        that. Called *after* a piggybacked Bambu-material-mapping
///        download (if this release publishes one) has already been
///        attempted (TASKS.md Nachtrag 2026-08-28, Nutzerwunsch:
///        Material-Mapping vor der Firmware aktualisieren).
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param downloadUrl Firmware ".bin" asset URL.
/// @param expectedHashHex Expected 64-hex-digit SHA-256 (lowercase) of the firmware.
/// @param errorOut Destination buffer receiving a human-readable error message on failure.
/// @param errorOutCapacity Size of `errorOut` in bytes.
/// @return true if the firmware was downloaded, verified, and flashed successfully.
// Eigene Funktion (statt weiterhin Teil von downloadUpdate()), damit ihre
// WiFiClientSecure/HTTPClient-Objekte (dataClient/dataHttp) beim Verlassen
// der Funktion vollstaendig zerstoert sind, bevor eine ggf. anschliessende
// Bambu-Material-Mapping-TLS-Verbindung aufgebaut wird -- siehe den
// mbedTLS-RAM-Kommentar in downloadUpdate().
bool downloadAndFlashFirmware(rtos::RtosContext& ctx, std::uint32_t requestId,
                              const char* downloadUrl,
                              const char* expectedHashHex, char* errorOut,
                              std::size_t errorOutCapacity) {
  FS_LOGI(services::LogComponent::Update,
          "Firmware download starting url=\"%s\" free_heap=%u", downloadUrl,
          static_cast<unsigned>(ESP.getFreeHeap()));
  WiFiClientSecure dataClient;
  dataClient.setInsecure();
  dataClient.setTimeout(config::kUpdateDownloadTimeoutMs);
  HTTPClient dataHttp;
  dataHttp.setConnectTimeout(static_cast<int32_t>(config::kUpdateDownloadTimeoutMs));
  dataHttp.setTimeout(static_cast<uint16_t>(config::kUpdateDownloadTimeoutMs));
  // GitHub-Asset-Downloads leiten per 302 auf eine host-fremde, signierte
  // URL (objects.githubusercontent.com) um.
  dataHttp.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!dataHttp.begin(dataClient, downloadUrl)) {
    std::snprintf(errorOut, errorOutCapacity,
                  "Verbindung zum Download fehlgeschlagen");
    return false;
  }
  dataHttp.addHeader("User-Agent", config::kUpdateUserAgent);
  const int dataStatus = dataHttp.GET();
  if (dataStatus != HTTP_CODE_OK) {
    std::snprintf(errorOut, errorOutCapacity, "Download fehlgeschlagen (%d)",
                  dataStatus);
    dataHttp.end();
    return false;
  }

  const int contentLength = dataHttp.getSize();
  const bool sizeKnown = contentLength > 0;
  if (!Update.begin(sizeKnown ? static_cast<std::size_t>(contentLength)
                              : UPDATE_SIZE_UNKNOWN)) {
    std::snprintf(errorOut, errorOutCapacity, "Speicherfehler: %s",
                  Update.errorString());
    FS_LOGE(services::LogComponent::Update, "Update.begin failed: %s",
            Update.errorString());
    dataHttp.end();
    return false;
  }

  // Manuelles Puffern statt der bequemen Update.write(stream)-Ueberladung
  // (Phase 13.3), da die Rohbytes fuer die laufende SHA-256-Berechnung
  // sichtbar sein muessen (Phase 13.4) -- Update.write(stream) liest intern
  // direkt vom Stream, ohne die Bytes an den Aufrufer zurueckzugeben.
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts_ret(&sha, 0);  // 0 = SHA-256 (nicht SHA-224)

  WiFiClient& stream = dataHttp.getStream();
  std::array<std::uint8_t, 1024> buffer;
  std::uint32_t lastProgressReportMs = millis();
  std::uint32_t lastDataMs = millis();
  bool connectionLost = false;
  for (;;) {
    if (sizeKnown && Update.remaining() == 0) break;
    if (!dataHttp.connected() && stream.available() == 0) break;
    const std::size_t available = stream.available();
    if (available == 0) {
      if (millis() - lastDataMs >= config::kUpdateStallTimeoutMs) {
        connectionLost = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    const std::size_t toRead = available < buffer.size() ? available : buffer.size();
    const std::size_t readBytes = stream.readBytes(buffer.data(), toRead);
    if (readBytes == 0) continue;
    const std::size_t written = Update.write(buffer.data(), readBytes);
    mbedtls_sha256_update_ret(&sha, buffer.data(), readBytes);
    if (written > 0) lastDataMs = millis();
    const std::uint32_t now = millis();
    if (now - lastProgressReportMs >= config::kUpdateProgressReportIntervalMs) {
      lastProgressReportMs = now;
      const std::size_t total = Update.size();
      const int percent =
          sizeKnown && total > 0
              ? static_cast<int>((Update.progress() * 100) / total)
              : 0;
      sendEvent(ctx, rtos::AppEventType::UpdateDownloadProgress, requestId,
                percent, "");
    }
  }
  dataHttp.end();

  std::uint8_t hash[32];
  mbedtls_sha256_finish_ret(&sha, hash);
  mbedtls_sha256_free(&sha);
  char actualHashHex[65];
  for (std::size_t index = 0; index < sizeof(hash); ++index) {
    std::snprintf(actualHashHex + index * 2, 3, "%02x", hash[index]);
  }

  const bool complete =
      connectionLost ? false
      : sizeKnown     ? Update.isFinished()
                      : Update.progress() > 0;
  if (!complete) {
    Update.abort();
    FS_LOGW(services::LogComponent::Update,
            "Download incomplete written=%u expected=%u",
            static_cast<unsigned>(Update.progress()),
            static_cast<unsigned>(Update.size()));
    std::snprintf(errorOut, errorOutCapacity,
                  "Verbindung w\xC3\xA4hrend des Downloads unterbrochen");
    return false;
  }

  if (std::strcmp(actualHashHex, expectedHashHex) != 0) {
    Update.abort();
    FS_LOGE(services::LogComponent::Update,
            "Checksum mismatch expected=%s actual=%s", expectedHashHex,
            actualHashHex);
    std::snprintf(errorOut, errorOutCapacity,
                  "Pr\xC3\xBC" "fsumme stimmt nicht \xC3\xBC" "berein");
    return false;
  }

  if (!Update.end(true) || Update.hasError()) {
    std::snprintf(errorOut, errorOutCapacity, "%s", Update.errorString());
    FS_LOGE(services::LogComponent::Update, "Update.end failed: %s", errorOut);
    return false;
  }

  FS_LOGI(services::LogComponent::Update,
          "Download complete bytes=%u checksum_verified=true free_heap=%u",
          static_cast<unsigned>(Update.progress()),
          static_cast<unsigned>(ESP.getFreeHeap()));
  return true;
}

/// @brief Core Bambu-material-mapping download: fetches the expected
///        SHA-256 from `checksumUrl`, downloads `downloadUrl` completely
///        into a PSRAM buffer, closes the TLS connection, and *then* sends
///        it to tasks::storageTask() in kStorageJsonPayloadCapacity-sized
///        chunks (StorageCommandType::BeginBambuMaterialDownload/
///        WriteBambuMaterialChunk/CommitBambuMaterialDownload). This task
///        never writes to the SD card itself (AGENTS.md "keine
///        SD-Zugriffe außerhalb StorageTask") -- StorageTask is the sole
///        authority on SHA-256 verification, JSON validation, and
///        activation, and (only when `reportEvents` is true) reports the
///        final success/failure event itself once
///        CommitBambuMaterialDownload finishes.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param downloadUrl "bambu_materials.json" asset URL.
/// @param checksumUrl "bambu_materials.json.sha256" asset URL.
/// @param reportEvents Whether to send BambuMaterialUpdateProgress/Result
///        AppEvents. True for a standalone, user-initiated download
///        (UpdateCommandType::DownloadBambuMaterials). False when
///        piggybacked onto a firmware update (see downloadUpdate()) --
///        there, only FS_LOGI/W/E lines report the outcome, so this silent
///        background download never pops its own progress overlay/result
///        dialog racing the firmware update's own restart confirmation;
///        failures here also must not affect the (already-successful)
///        firmware update's own reported result.
// Downloads the *entire* file into a PSRAM buffer before sending anything
// to StorageTask, deliberately not streaming chunk-by-chunk while the TLS
// connection is still open (an earlier version did). On real hardware,
// running the HTTPS/TLS download (UpdateTask) concurrently with the SD
// writes it triggered (StorageTask) produced SD File::write() calls that
// got permanently stuck returning 0 bytes partway through a chunk, not a
// transient hiccup a retry could recover from (Nutzerbericht 2026-08-28,
// TASKS.md) -- consistent with ESP32-S3's TLS/crypto acceleration and its
// SD/SPI DMA path sharing the same underlying GDMA hardware, which does
// not tolerate concurrent use from two tasks. The file is at most
// kBambuMaterialsMaxFileSize (16 KiB) -- trivial to hold in PSRAM (free
// heap stayed above 30 KiB throughout in every observed log) -- so
// buffering it whole and only starting any SD-related StorageCommand
// *after* dataHttp.end() has fully torn down the TLS session avoids the
// concurrency entirely, at the cost of a few seconds' extra latency before
// the first byte reaches the SD card.
// Reuses one static rtos::StorageCommand instance for every Begin/Write/
// Commit send below, instead of a fresh ~880-byte struct local per call
// site -- xQueueSend() copies it into the queue immediately, so
// overwriting/resending it right after is safe (this function's task,
// UpdateTask, only ever processes one command/download at a time, see
// updateTask()'s single-threaded receive loop).
rtos::StorageCommand& sharedBambuMaterialCommand() {
  static rtos::StorageCommand command{};
  return command;
}

void streamBambuMaterialsFromUrls(rtos::RtosContext& ctx,
                                  std::uint32_t requestId,
                                  const char* downloadUrl,
                                  const char* checksumUrl, bool reportEvents) {
  auto reportFailure = [&](const char* text) {
    FS_LOGW(services::LogComponent::Update,
            "Bambu material mapping update failed reason=\"%s\"", text);
    if (reportEvents) {
      sendEvent(ctx, rtos::AppEventType::BambuMaterialUpdateResult, requestId,
                0, text);
    }
  };

  FS_LOGI(services::LogComponent::Update,
          "Bambu material mapping update starting url=\"%s\" "
          "checksum_url=\"%s\" free_heap=%u",
          downloadUrl, checksumUrl, static_cast<unsigned>(ESP.getFreeHeap()));

  char expectedHashHex[65];
  if (!fetchChecksum(checksumUrl, expectedHashHex)) {
    reportFailure("Pr\xC3\xBC" "fsumme konnte nicht geladen werden");
    return;
  }
  FS_LOGI(services::LogComponent::Update,
          "Bambu material mapping checksum fetched sha256=\"%s\" "
          "free_heap=%u",
          expectedHashHex, static_cast<unsigned>(ESP.getFreeHeap()));

  WiFiClientSecure dataClient;
  dataClient.setInsecure();
  dataClient.setTimeout(config::kUpdateDownloadTimeoutMs);
  HTTPClient dataHttp;
  dataHttp.setConnectTimeout(static_cast<int32_t>(config::kUpdateDownloadTimeoutMs));
  dataHttp.setTimeout(static_cast<uint16_t>(config::kUpdateDownloadTimeoutMs));
  dataHttp.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!dataHttp.begin(dataClient, downloadUrl)) {
    reportFailure("Verbindung zum Download fehlgeschlagen");
    return;
  }
  dataHttp.addHeader("User-Agent", config::kUpdateUserAgent);
  const int dataStatus = dataHttp.GET();
  if (dataStatus != HTTP_CODE_OK) {
    char error[64];
    std::snprintf(error, sizeof(error), "Download fehlgeschlagen (%d)", dataStatus);
    dataHttp.end();
    reportFailure(error);
    return;
  }

  const int contentLength = dataHttp.getSize();
  const bool sizeKnown = contentLength > 0;
  FS_LOGI(services::LogComponent::Update,
          "Bambu material mapping connection opened content_length=%d "
          "size_known=%d free_heap=%u",
          contentLength, sizeKnown ? 1 : 0,
          static_cast<unsigned>(ESP.getFreeHeap()));
  if (sizeKnown &&
      static_cast<std::size_t>(contentLength) > config::kBambuMaterialsMaxFileSize) {
    dataHttp.end();
    reportFailure("Heruntergeladene Datei ist zu gro\xC3\x9F");
    return;
  }

  // Static PSRAM buffer (see services/PsramAlloc.h) -- 16 KiB is far too
  // large for a task-stack local, and reused across calls the same way
  // sendEvent()/sharedBambuMaterialCommand() already do for their own
  // oversized static instances.
  using BambuMaterialsBuffer =
      std::array<std::uint8_t, config::kBambuMaterialsMaxFileSize>;
  static BambuMaterialsBuffer* downloadBuffer =
      services::allocatePsramInstance<BambuMaterialsBuffer>(
          "UpdateTask.bambuMaterialsBuffer");

  WiFiClient& stream = dataHttp.getStream();
  std::size_t totalBytes = 0;
  std::uint32_t lastProgressReportMs = millis();
  std::uint32_t lastDataMs = millis();
  bool connectionLost = false;
  bool tooLarge = false;
  for (;;) {
    if (sizeKnown && totalBytes >= static_cast<std::size_t>(contentLength)) break;
    if (!dataHttp.connected() && stream.available() == 0) break;
    const std::size_t available = stream.available();
    if (available == 0) {
      if (millis() - lastDataMs >= config::kUpdateStallTimeoutMs) {
        connectionLost = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    const std::size_t remainingCapacity = downloadBuffer->size() - totalBytes;
    if (remainingCapacity == 0) {
      tooLarge = true;
      break;
    }
    const std::size_t toRead =
        available < remainingCapacity ? available : remainingCapacity;
    const std::size_t readBytes =
        stream.readBytes(downloadBuffer->data() + totalBytes, toRead);
    if (readBytes == 0) continue;
    lastDataMs = millis();
    totalBytes += readBytes;

    const std::uint32_t now = millis();
    if (reportEvents &&
        now - lastProgressReportMs >= config::kUpdateProgressReportIntervalMs) {
      lastProgressReportMs = now;
      const int percent =
          sizeKnown ? static_cast<int>(
                          (totalBytes * 100) /
                          static_cast<std::size_t>(contentLength))
                    : 0;
      sendEvent(ctx, rtos::AppEventType::BambuMaterialUpdateProgress,
                requestId, percent, "");
    }
  }
  dataHttp.end();  // TLS session fully torn down before any SD-related command is sent below.

  if (tooLarge) {
    reportFailure("Heruntergeladene Datei ist zu gro\xC3\x9F");
    return;
  }
  if (connectionLost || totalBytes == 0 ||
      (sizeKnown && totalBytes < static_cast<std::size_t>(contentLength))) {
    reportFailure("Verbindung w\xC3\xA4hrend des Downloads unterbrochen");
    return;
  }

  FS_LOGI(services::LogComponent::Update,
          "Bambu material mapping download completed bytes=%u "
          "sha256_validation=pending free_heap=%u",
          static_cast<unsigned>(totalBytes),
          static_cast<unsigned>(ESP.getFreeHeap()));

  // Nothing was sent to StorageTask before this point -- a failure above
  // simply returns, leaving the active file/RAM cache untouched, no Abort
  // needed. From here on, Begin/Write/Commit are sent back-to-back with no
  // further network/TLS activity interleaved.
  rtos::StorageCommand& command = sharedBambuMaterialCommand();
  command = rtos::StorageCommand{};
  command.type = rtos::StorageCommandType::BeginBambuMaterialDownload;
  command.requestId = requestId;
  sendStorageCommand(ctx, command);
  FS_LOGD(services::LogComponent::Update,
          "Bambu material mapping Begin command sent request_id=%lu",
          static_cast<unsigned long>(requestId));

  std::size_t offset = 0;
  while (offset < totalBytes) {
    const std::size_t remaining = totalBytes - offset;
    const std::size_t chunkSize =
        remaining < sizeof(command.json) ? remaining : sizeof(command.json);
    command.type = rtos::StorageCommandType::WriteBambuMaterialChunk;
    command.requestId = requestId;
    command.jsonLength = static_cast<std::uint16_t>(chunkSize);
    std::memcpy(command.json, downloadBuffer->data() + offset, chunkSize);
    sendStorageCommand(ctx, command);
    offset += chunkSize;
    FS_LOGD(services::LogComponent::Update,
            "Bambu material mapping chunk sent bytes=%u total=%u",
            static_cast<unsigned>(chunkSize), static_cast<unsigned>(offset));
  }

  // No BambuMaterialUpdateResult sent from here on success -- StorageTask is
  // the sole authority on SHA-256/JSON validation and activation, and (if
  // reportEvents) reports the final result itself once
  // CommitBambuMaterialDownload finishes (see StorageTask.cpp). In the
  // silent/piggybacked case, StorageTask's own FS_LOGI/W lines are the only
  // record of the final outcome.
  command = rtos::StorageCommand{};
  command.type = rtos::StorageCommandType::CommitBambuMaterialDownload;
  command.requestId = requestId;
  command.jsonLength = static_cast<std::uint16_t>(std::strlen(expectedHashHex));
  std::snprintf(command.json, sizeof(command.json), "%s", expectedHashHex);
  sendStorageCommand(ctx, command);
  FS_LOGD(services::LogComponent::Update,
          "Bambu material mapping Commit command sent request_id=%lu "
          "expected_sha256=\"%s\"",
          static_cast<unsigned long>(requestId), expectedHashHex);

  // Blocks until StorageTask signals it has fully finished processing the
  // Commit (success or failure) -- this function's caller (downloadUpdate())
  // otherwise proceeds straight to its own next TLS connection (the
  // firmware download) immediately after returning, which on real hardware
  // reliably stalled a still-in-flight SD write in StorageTask (ESP32-S3
  // shares GDMA hardware between TLS/crypto acceleration and SD/SPI DMA,
  // Nutzerbericht 2026-08-28, TASKS.md). Bounded by
  // kBambuMaterialCommitWaitTimeoutMs so a stuck StorageTask can never hang
  // this task forever; StorageTask's own FS_LOGI/W/E lines already report
  // the actual outcome regardless of whether this wait times out. Drains
  // any stale "given" state first -- a previous call's wait could have
  // timed out just before StorageTask actually finished, leaving the
  // semaphore given with nobody having taken it; without this, that stale
  // give would let *this* wait return immediately without really waiting.
  xSemaphoreTake(ctx.bambuMaterialDownloadDone, 0);
  if (xSemaphoreTake(ctx.bambuMaterialDownloadDone,
                     pdMS_TO_TICKS(config::kBambuMaterialCommitWaitTimeoutMs)) !=
      pdTRUE) {
    FS_LOGW(services::LogComponent::Update,
            "Bambu material mapping commit wait timed out request_id=%lu",
            static_cast<unsigned long>(requestId));
  }
}

/// @brief Handles UpdateCommandType::DownloadUpdate: fetches the latest
///        release's firmware asset and checksum, downloads and flashes it
///        via the OTA Update library, verifying the SHA-256 before
///        committing. If the same release also publishes
///        bambu_materials.json/bambu_materials.json.sha256, downloads and
///        activates that *first*, before touching the firmware (TASKS.md
///        Nachtrag 2026-08-28, Nutzerwunsch: Material-Mapping vor der
///        Firmware aktualisieren) -- silently skipped if this release
///        doesn't publish it (older releases), and never allowed to block
///        or fail the firmware installation.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
// Loest eine eigene, frische Abfrage der Update-Quelle aus (kein Zustand
// aus einer vorherigen checkForUpdate()-Anfrage wird wiederverwendet --
// vermeidet eine veraltete Download-URL, siehe UpdateCommandType).
void downloadUpdate(rtos::RtosContext& ctx, std::uint32_t requestId) {
  // Jede TLS-Verbindung (Release-Metadaten, Pruefsumme, eigentlicher
  // Download) steht in einem eigenen Block/einer eigenen Funktion:
  // WiFiClientSecure/HTTPClient muessen vor dem Aufbau der naechsten
  // Verbindung vollstaendig zerstoert sein, sonst haelt der interne RAM
  // zwei mbedTLS-Sitzungen gleichzeitig vor. Genau das fuehrte zu
  // "esp-sha: Failed to allocate buf memory" beim Pruefsummen-Abruf, weil
  // metaClient/metaHttp trotz metaHttp.end() bis zum Ende der Funktion am
  // Leben geblieben waeren (Fund 2026-08-24) -- downloadAndFlashFirmware()
  // ist deshalb eine eigene Funktion, nicht mehr Teil dieser hier.
  char downloadUrl[256]{};
  char checksumFetchUrl[256]{};
  // Optional: leer, falls dieses Release keine bambu_materials.json/
  // .sha256-Assets veroeffentlicht -- dann wird der Materialteil unten
  // einfach uebersprungen, kein Fehler.
  char bambuMaterialsUrl[256]{};
  char bambuMaterialsChecksumUrl[256]{};
  {
    char releaseUrl[128];
    std::snprintf(releaseUrl, sizeof(releaseUrl),
                  "https://%s/repos/%s/%s/releases/latest",
                  config::kUpdateApiHost, config::kUpdateRepoOwner,
                  config::kUpdateRepoName);

    WiFiClientSecure metaClient;
    metaClient.setInsecure();
    metaClient.setTimeout(config::kUpdateCheckTimeoutMs);
    HTTPClient metaHttp;
    metaHttp.setConnectTimeout(static_cast<int32_t>(config::kUpdateCheckTimeoutMs));
    metaHttp.setTimeout(static_cast<uint16_t>(config::kUpdateCheckTimeoutMs));
    if (!metaHttp.begin(metaClient, releaseUrl)) {
      sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
                "Verbindung zur Update-Quelle fehlgeschlagen");
      return;
    }
    metaHttp.addHeader("User-Agent", config::kUpdateUserAgent);
    const int metaStatus = metaHttp.GET();
    if (metaStatus != HTTP_CODE_OK) {
      char error[64];
      std::snprintf(error, sizeof(error), "HTTP-Anfrage fehlgeschlagen (%d)",
                    metaStatus);
      metaHttp.end();
      sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0, error);
      return;
    }

    // Nur die Asset-Liste behalten (Name + Download-URL je Anhang) -- gleiche
    // Filter-Idee wie in checkForUpdate().
    JsonDocument filter;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(
        document, metaHttp.getStream(), DeserializationOption::Filter(filter));
    metaHttp.end();
    if (jsonError) {
      sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
                "Ung\xC3\xBCltige Serverantwort");
      return;
    }

    // Erstes Asset mit .bin-Endung (Nutzerentscheidung 2026-08-25). Ein
    // einzelner Durchlauf reicht fuer .bin *und* die optionalen
    // Material-Mapping-Assets -- kein break beim ersten .bin-Treffer mehr,
    // damit auch spaeter im Array gelistete Material-Mapping-Assets nicht
    // uebersprungen werden.
    const char* assetUrl = nullptr;
    char binAssetName[128]{};
    for (JsonObject asset : document["assets"].as<JsonArray>()) {
      const char* name = asset["name"];
      if (name == nullptr) continue;
      const char* url = asset["browser_download_url"];
      const std::size_t length = std::strlen(name);
      if (assetUrl == nullptr && length >= 4 &&
          std::strcmp(name + length - 4, ".bin") == 0) {
        assetUrl = url;
        std::snprintf(binAssetName, sizeof(binAssetName), "%s", name);
      } else if (std::strcmp(name, "bambu_materials.json") == 0) {
        std::snprintf(bambuMaterialsUrl, sizeof(bambuMaterialsUrl), "%s",
                      url != nullptr ? url : "");
      } else if (std::strcmp(name, "bambu_materials.json.sha256") == 0) {
        std::snprintf(bambuMaterialsChecksumUrl,
                      sizeof(bambuMaterialsChecksumUrl), "%s",
                      url != nullptr ? url : "");
      }
    }
    if (assetUrl == nullptr) {
      sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
                "Kein Firmware-Anhang gefunden");
      return;
    }
    std::snprintf(downloadUrl, sizeof(downloadUrl), "%s", assetUrl);

    // Pruefsumme (TASKS.md Phase 13.4, Nutzerentscheidung 2026-08-25): zweites
    // Asset "<binAssetName>.sha256", enthaelt nur den 64-stelligen Hex-Hash.
    // Faehlt fehlend -> Update wird sicherheitshalber NICHT installiert
    // (fail closed), nicht stillschweigend uebersprungen.
    char checksumAssetName[144];
    std::snprintf(checksumAssetName, sizeof(checksumAssetName), "%s.sha256",
                  binAssetName);
    const char* checksumUrl = nullptr;
    for (JsonObject asset : document["assets"].as<JsonArray>()) {
      const char* name = asset["name"];
      if (name != nullptr && std::strcmp(name, checksumAssetName) == 0) {
        checksumUrl = asset["browser_download_url"];
        break;
      }
    }
    if (checksumUrl == nullptr) {
      FS_LOGW(services::LogComponent::Update,
              "Checksum asset not found expected_name=\"%s\"",
              checksumAssetName);
      sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
                "Keine Pr\xC3\xBC" "fsumme ver\xC3\xB6" "ffentlicht");
      return;
    }
    std::snprintf(checksumFetchUrl, sizeof(checksumFetchUrl), "%s", checksumUrl);
  }  // metaClient, metaHttp, filter, document werden hier zerstoert.

  // Material-Mapping-Download VOR der Firmware (Nutzerwunsch 2026-08-28):
  // erst die (kleinere, risikoärmere) Material-Zuordnung schreiben/
  // aktivieren, danach die Firmware selbst herunterladen/flashen. Ein
  // Fehlschlag hier blockiert die Firmware-Installation nicht -- nur ein
  // Log, kein Abbruch. free_heap wird an mehreren Stellen mitgeloggt, um
  // einen frueheren Stack-Overflow (Nutzerbericht 2026-08-28, siehe
  // TASKS.md) von einem moeglichen Speicherproblem unterscheiden zu koennen.
  FS_LOGI(services::LogComponent::Update,
          "downloadUpdate free_heap=%u", static_cast<unsigned>(ESP.getFreeHeap()));
  if (bambuMaterialsUrl[0] != '\0' && bambuMaterialsChecksumUrl[0] != '\0') {
    FS_LOGI(services::LogComponent::Update,
            "Bambu material mapping bundled with this release, updating "
            "before firmware");
    streamBambuMaterialsFromUrls(ctx, requestId, bambuMaterialsUrl,
                                 bambuMaterialsChecksumUrl,
                                 /*reportEvents=*/false);
  } else {
    FS_LOGI(services::LogComponent::Update,
            "Bambu material mapping not published in this release, skipping");
  }
  FS_LOGI(services::LogComponent::Update,
          "downloadUpdate proceeding to firmware free_heap=%u",
          static_cast<unsigned>(ESP.getFreeHeap()));

  char expectedHashHex[65];
  if (!fetchChecksum(checksumFetchUrl, expectedHashHex)) {
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              "Pr\xC3\xBC" "fsumme konnte nicht geladen werden");
    return;
  }

  char firmwareError[96]{};
  if (!downloadAndFlashFirmware(ctx, requestId, downloadUrl, expectedHashHex,
                                firmwareError, sizeof(firmwareError))) {
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              firmwareError);
    return;
  }

  sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 1, "");
}

/// @brief Handles UpdateCommandType::DownloadBambuMaterials: fetches
///        bambu_materials.json/bambu_materials.json.sha256 from the latest
///        release and streams them to tasks::storageTask() (see
///        streamBambuMaterialsFromUrls()). Standalone/user-initiated path
///        -- reports progress/result events (`reportEvents=true`), unlike
///        the piggybacked download inside downloadUpdate().
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
// Eigene, frische releases/latest-Abfrage (kein Zustand aus einem
// vorherigen DownloadUpdate wiederverwendet), analog zu checkForUpdate()/
// downloadUpdate().
void downloadBambuMaterials(rtos::RtosContext& ctx, std::uint32_t requestId) {
  char downloadUrl[256]{};
  char checksumFetchUrl[256]{};
  {
    char releaseUrl[128];
    std::snprintf(releaseUrl, sizeof(releaseUrl),
                  "https://%s/repos/%s/%s/releases/latest",
                  config::kUpdateApiHost, config::kUpdateRepoOwner,
                  config::kUpdateRepoName);

    WiFiClientSecure metaClient;
    metaClient.setInsecure();
    metaClient.setTimeout(config::kUpdateCheckTimeoutMs);
    HTTPClient metaHttp;
    metaHttp.setConnectTimeout(static_cast<int32_t>(config::kUpdateCheckTimeoutMs));
    metaHttp.setTimeout(static_cast<uint16_t>(config::kUpdateCheckTimeoutMs));
    if (!metaHttp.begin(metaClient, releaseUrl)) {
      sendEvent(ctx, rtos::AppEventType::BambuMaterialUpdateResult, requestId,
                0, "Verbindung zur Update-Quelle fehlgeschlagen");
      return;
    }
    metaHttp.addHeader("User-Agent", config::kUpdateUserAgent);
    const int metaStatus = metaHttp.GET();
    if (metaStatus != HTTP_CODE_OK) {
      char error[64];
      std::snprintf(error, sizeof(error), "HTTP-Anfrage fehlgeschlagen (%d)",
                    metaStatus);
      metaHttp.end();
      sendEvent(ctx, rtos::AppEventType::BambuMaterialUpdateResult, requestId,
                0, error);
      return;
    }

    JsonDocument filter;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(
        document, metaHttp.getStream(), DeserializationOption::Filter(filter));
    metaHttp.end();
    if (jsonError) {
      sendEvent(ctx, rtos::AppEventType::BambuMaterialUpdateResult, requestId,
                0, "Ung\xC3\xBCltige Serverantwort");
      return;
    }

    // Both bambu_materials.json and its .sha256 sidecar must be published as
    // release assets (exact names, see scripts/release.ps1/docs/bambu-protocol.md)
    // -- fail closed if either is missing, same policy as the firmware asset.
    const char* assetUrl = nullptr;
    const char* checksumUrl = nullptr;
    for (JsonObject asset : document["assets"].as<JsonArray>()) {
      const char* name = asset["name"];
      if (name == nullptr) continue;
      if (std::strcmp(name, "bambu_materials.json") == 0) {
        assetUrl = asset["browser_download_url"];
      } else if (std::strcmp(name, "bambu_materials.json.sha256") == 0) {
        checksumUrl = asset["browser_download_url"];
      }
    }
    if (assetUrl == nullptr || checksumUrl == nullptr) {
      FS_LOGW(services::LogComponent::Update,
              "Bambu material mapping asset not found url=\"%s\"", releaseUrl);
      sendEvent(ctx, rtos::AppEventType::BambuMaterialUpdateResult, requestId,
                0, "Kein Material-Mapping-Anhang gefunden");
      return;
    }
    std::snprintf(downloadUrl, sizeof(downloadUrl), "%s", assetUrl);
    std::snprintf(checksumFetchUrl, sizeof(checksumFetchUrl), "%s", checksumUrl);
  }  // metaClient, metaHttp, filter, document werden hier zerstoert.

  streamBambuMaterialsFromUrls(ctx, requestId, downloadUrl, checksumFetchUrl,
                               /*reportEvents=*/true);
}

}  // namespace

void updateTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::UpdateCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.updateCommandQueue, &command, portMAX_DELAY) ==
        pdTRUE) {
      switch (command.type) {
        case rtos::UpdateCommandType::CheckForUpdate:
          checkForUpdate(ctx, command.requestId);
          break;
        case rtos::UpdateCommandType::DownloadUpdate:
          downloadUpdate(ctx, command.requestId);
          break;
        case rtos::UpdateCommandType::DownloadBambuMaterials:
          downloadBambuMaterials(ctx, command.requestId);
          break;
      }
    }
  }
}

}  // namespace filament_station::tasks
