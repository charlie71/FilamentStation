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
#include <cctype>
#include <cstdio>
#include <cstring>

#include "config/AppConfig.h"
#include "config/UpdateConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"
#include "services/PsramAlloc.h"
#include "services/SemVer.h"

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

/// @brief Extracts a lowercase 64-hex-digit SHA-256 from the start of a checksum-file body.
/// @param text Checksum file content (may have " filename" trailing, like `sha256sum` output).
/// @param out Destination buffer, at least 65 bytes (64 hex digits + NUL).
/// @return false if `text` does not start with 64 hex digits.
// Prueft, ob "text" mit genau 64 Hex-Ziffern beginnt (ein SHA-256-Hash in
// Hex-Darstellung) und kopiert sie kleingeschrieben nach out (65 Byte,
// inkl. Nullterminator). sha256sum-Ausgaben haben oft noch " filename"
// dahinter -- das wird ignoriert, nur die ersten 64 Zeichen zaehlen.
bool extractHexSha256(const char* text, char* out) {
  std::size_t index = 0;
  for (; index < 64; ++index) {
    const unsigned char c = static_cast<unsigned char>(text[index]);
    if (c == '\0' || std::isxdigit(c) == 0) return false;
    out[index] = static_cast<char>(std::tolower(c));
  }
  out[64] = '\0';
  return true;
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
  return extractHexSha256(body.c_str(), outHex64);
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

/// @brief Handles UpdateCommandType::DownloadUpdate: fetches the latest
///        release's firmware asset and checksum, downloads and flashes it
///        via the OTA Update library, verifying the SHA-256 before committing.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
// Loest eine eigene, frische Abfrage der Update-Quelle aus (kein Zustand
// aus einer vorherigen checkForUpdate()-Anfrage wird wiederverwendet --
// vermeidet eine veraltete Download-URL, siehe UpdateCommandType).
void downloadUpdate(rtos::RtosContext& ctx, std::uint32_t requestId) {
  // Jede TLS-Verbindung (Release-Metadaten, Pruefsumme, eigentlicher
  // Download) steht in einem eigenen Block: WiFiClientSecure/HTTPClient
  // muessen vor dem Aufbau der naechsten Verbindung vollstaendig zerstoert
  // sein, sonst haelt der interne RAM zwei mbedTLS-Sitzungen gleichzeitig
  // vor. Genau das fuehrte zu "esp-sha: Failed to allocate buf memory"
  // beim Pruefsummen-Abruf, weil metaClient/metaHttp trotz metaHttp.end()
  // bis zum Ende der Funktion am Leben geblieben waeren (Fund 2026-08-24).
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

    // Erstes Asset mit .bin-Endung (Nutzerentscheidung 2026-08-25).
    const char* assetUrl = nullptr;
    char binAssetName[128]{};
    for (JsonObject asset : document["assets"].as<JsonArray>()) {
      const char* name = asset["name"];
      if (name == nullptr) continue;
      const std::size_t length = std::strlen(name);
      if (length >= 4 && std::strcmp(name + length - 4, ".bin") == 0) {
        assetUrl = asset["browser_download_url"];
        std::snprintf(binAssetName, sizeof(binAssetName), "%s", name);
        break;
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

  char expectedHashHex[65];
  if (!fetchChecksum(checksumFetchUrl, expectedHashHex)) {
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              "Pr\xC3\xBC" "fsumme konnte nicht geladen werden");
    return;
  }

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
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              "Verbindung zum Download fehlgeschlagen");
    return;
  }
  dataHttp.addHeader("User-Agent", config::kUpdateUserAgent);
  const int dataStatus = dataHttp.GET();
  if (dataStatus != HTTP_CODE_OK) {
    char error[64];
    std::snprintf(error, sizeof(error), "Download fehlgeschlagen (%d)", dataStatus);
    dataHttp.end();
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0, error);
    return;
  }

  const int contentLength = dataHttp.getSize();
  const bool sizeKnown = contentLength > 0;
  if (!Update.begin(sizeKnown ? static_cast<std::size_t>(contentLength)
                              : UPDATE_SIZE_UNKNOWN)) {
    char error[96];
    std::snprintf(error, sizeof(error), "Speicherfehler: %s",
                  Update.errorString());
    FS_LOGE(services::LogComponent::Update, "Update.begin failed: %s",
            Update.errorString());
    dataHttp.end();
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0, error);
    return;
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
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              "Verbindung w\xC3\xA4hrend des Downloads unterbrochen");
    return;
  }

  if (std::strcmp(actualHashHex, expectedHashHex) != 0) {
    Update.abort();
    FS_LOGE(services::LogComponent::Update,
            "Checksum mismatch expected=%s actual=%s", expectedHashHex,
            actualHashHex);
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              "Pr\xC3\xBC" "fsumme stimmt nicht \xC3\xBC" "berein");
    return;
  }

  if (!Update.end(true) || Update.hasError()) {
    char error[96];
    std::snprintf(error, sizeof(error), "%s", Update.errorString());
    FS_LOGE(services::LogComponent::Update, "Update.end failed: %s", error);
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0, error);
    return;
  }

  FS_LOGI(services::LogComponent::Update,
          "Download complete bytes=%u checksum_verified=true",
          static_cast<unsigned>(Update.progress()));
  sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 1, "");
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
      }
    }
  }
}

}  // namespace filament_station::tasks
