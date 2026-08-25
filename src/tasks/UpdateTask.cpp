#include "tasks/Tasks.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

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

// Loest eine eigene, frische Abfrage der Update-Quelle aus (kein Zustand
// aus einer vorherigen checkForUpdate()-Anfrage wird wiederverwendet --
// vermeidet eine veraltete Download-URL, siehe UpdateCommandType).
void downloadUpdate(rtos::RtosContext& ctx, std::uint32_t requestId) {
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
  for (JsonObject asset : document["assets"].as<JsonArray>()) {
    const char* name = asset["name"];
    if (name == nullptr) continue;
    const std::size_t length = std::strlen(name);
    if (length >= 4 && std::strcmp(name + length - 4, ".bin") == 0) {
      assetUrl = asset["browser_download_url"];
      break;
    }
  }
  if (assetUrl == nullptr) {
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0,
              "Kein Firmware-Anhang gefunden");
    return;
  }
  char downloadUrl[256];
  std::snprintf(downloadUrl, sizeof(downloadUrl), "%s", assetUrl);

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

  WiFiClient& stream = dataHttp.getStream();
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
    const std::size_t written = Update.write(stream);
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

  if (!Update.end(true) || Update.hasError()) {
    char error[96];
    std::snprintf(error, sizeof(error), "%s", Update.errorString());
    FS_LOGE(services::LogComponent::Update, "Update.end failed: %s", error);
    sendEvent(ctx, rtos::AppEventType::UpdateDownloadResult, requestId, 0, error);
    return;
  }

  FS_LOGI(services::LogComponent::Update, "Download complete bytes=%u",
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
