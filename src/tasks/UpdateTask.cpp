#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <cstdio>

#include "config/AppConfig.h"
#include "config/UpdateConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"
#include "services/PsramAlloc.h"
#include "services/SemVer.h"

namespace filament_station::tasks {
namespace {

void sendResult(rtos::RtosContext& ctx, std::uint32_t requestId,
                std::int32_t value, const char* text) {
  // static, PSRAM-backed (services/PsramAlloc.h): siehe SpoolmanTask::
  // sendResult() fuer die Begruendung (AppEvent ist gross, dieser Task
  // sendet immer nur ein Ergebnis pro Anfrage).
  static rtos::AppEvent* event = services::allocatePsramInstance<rtos::AppEvent>(
      "UpdateTask.sendResult");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::UpdateCheckResult;
  event->requestId = requestId;
  event->value = value;
  std::snprintf(event->text, sizeof(event->text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Update,
            "Event enqueue failed queue=app_event result=check");
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
    sendResult(ctx, requestId, -1, "Verbindung zur Update-Quelle fehlgeschlagen");
    return;
  }
  // GitHub lehnt Anfragen ohne User-Agent-Header ab.
  http.addHeader("User-Agent", config::kUpdateUserAgent);

  const int status = http.GET();
  if (status == 404) {
    http.end();
    FS_LOGI(services::LogComponent::Update,
            "No releases published yet url=\"%s\"", url);
    sendResult(ctx, requestId, -1, "Keine Version ver\xC3\xB6" "ffentlicht");
    return;
  }
  if (status != HTTP_CODE_OK) {
    char error[64];
    std::snprintf(error, sizeof(error), "HTTP-Anfrage fehlgeschlagen (%d)", status);
    FS_LOGW(services::LogComponent::Update,
            "Request failed method=GET url=\"%s\" status=%d", url, status);
    http.end();
    sendResult(ctx, requestId, -1, error);
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
    sendResult(ctx, requestId, -1, "Ung\xC3\xBCltige Serverantwort");
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
    sendResult(ctx, requestId, -1, "Version nicht auswertbar");
    return;
  }

  if (services::compareSemVer(latest, current) > 0) {
    FS_LOGI(services::LogComponent::Update, "Update available tag=\"%s\"",
            tagName);
    sendResult(ctx, requestId, 1, tagName);
  } else {
    FS_LOGI(services::LogComponent::Update, "Firmware up to date tag=\"%s\"",
            tagName);
    sendResult(ctx, requestId, 0, "");
  }
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
      }
    }
  }
}

}  // namespace filament_station::tasks
