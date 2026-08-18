#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cstdio>
#include <cstring>

#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
namespace {
models::SpoolmanSettings activeSettings{};

void sendResult(rtos::RtosContext& ctx, rtos::AppEventType type,
                std::uint32_t requestId, const char* text) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: result queue overflow");
}

bool getJson(const char* url, std::uint32_t timeoutMs, JsonDocument& document,
             char* error, std::size_t errorCapacity) {
  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(timeoutMs));
  http.setTimeout(static_cast<uint16_t>(timeoutMs));
  if (!http.begin(url)) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Server-URL");
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    std::snprintf(error, errorCapacity, "HTTP-Anfrage fehlgeschlagen (%d)",
                  status);
    http.end();
    return false;
  }
  const DeserializationError jsonError = deserializeJson(document, http.getStream());
  http.end();
  if (jsonError) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Serverantwort");
    return false;
  }
  return true;
}

void healthCheck(rtos::RtosContext& ctx, const rtos::SpoolmanCommand& command) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_WIFI_CONNECTED) == 0) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Keine WLAN-Verbindung");
    return;
  }
  char url[160]{};
  char error[96]{};
  JsonDocument health;
  std::snprintf(url, sizeof(url), "%s/health", command.settings.serverUrl);
  if (!getJson(url, command.settings.timeoutMs, health, error, sizeof(error))) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }

  JsonDocument info;
  std::snprintf(url, sizeof(url), "%s/info", command.settings.serverUrl);
  if (!getJson(url, command.settings.timeoutMs, info, error, sizeof(error))) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }
  const char* version = info["version"] | "unbekannt";
  char message[96]{};
  std::snprintf(message, sizeof(message), "Online | Version %s", version);
  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
  sendResult(ctx, rtos::AppEventType::SpoolmanConnected, command.requestId,
             message);
}
}  // namespace

void spoolmanTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::SpoolmanCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.spoolmanCommandQueue, &command, portMAX_DELAY) != pdPASS)
      continue;
    if (command.type == rtos::SpoolmanCommandType::ApplyConfiguration) {
      activeSettings = command.settings;
      if (!activeSettings.enabled)
        xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    } else if (command.type == rtos::SpoolmanCommandType::HealthCheck) {
      healthCheck(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::ImportTagDefinition) {
      sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
                 "Spoolman-Import ist erst in einer sp\xC3\xA4teren Phase verf\xC3\xBCgbar");
    }
  }
}
}  // namespace filament_station::tasks
