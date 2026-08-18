#include "rtos/RtosContext.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "config/NetworkConfig.h"
#include "config/TaskConfig.h"
#include "rtos/Messages.h"
#include "tasks/Tasks.h"

namespace filament_station::rtos {
namespace {
RtosContext instance;
static_assert(sizeof(LogMessage::text) == config::kLogMessageCapacity);

bool createTask(TaskFunction_t function, const config::TaskSettings& settings,
                RtosContext* taskContext, TaskHandle_t* handle) {
  return xTaskCreatePinnedToCore(function, settings.name, settings.stackSize,
                                 taskContext, settings.priority, handle,
                                 settings.core) == pdPASS;
}

void loggingTaskMain(void* parameter) {
  auto& ctx = *static_cast<RtosContext*>(parameter);
  static LogMessage message{};
  static constexpr std::uint8_t newline[] = {'\r', '\n'};
  for (;;) {
    if (xQueueReceive(ctx.logQueue, &message, portMAX_DELAY) != pdPASS) continue;
    const std::size_t length = strnlen(message.text, sizeof(message.text));
    std::size_t written = 0;
    while (written < length) {
      const std::size_t count = Serial.write(
          reinterpret_cast<const std::uint8_t*>(message.text) + written,
          length - written);
      if (count == 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
      } else {
        written += count;
      }
    }
    written = 0;
    while (written < sizeof(newline)) {
      const std::size_t count =
          Serial.write(newline + written, sizeof(newline) - written);
      if (count == 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
      } else {
        written += count;
      }
    }
    Serial.flush();
  }
}
}  // namespace

RtosContext& context() { return instance; }

void logLine(const char* message) {
  auto& ctx = context();
  if (message == nullptr || ctx.logQueue == nullptr) return;
  LogMessage entry{};
  std::snprintf(entry.text, sizeof(entry.text), "%s", message);
  // Eine volle Queue verwirft nur die komplette neue Zeile. Bereits laufende
  // USB-Ausgaben werden niemals von einem anderen Task unterbrochen.
  xQueueSend(ctx.logQueue, &entry, pdMS_TO_TICKS(10));
}

void logf(const char* format, ...) {
  if (format == nullptr) return;
  char line[config::kLogMessageCapacity]{};
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(line, sizeof(line), format, arguments);
  va_end(arguments);
  logLine(line);
}

bool RtosContext::createObjects() {
  appEventQueue = xQueueCreate(config::kAppEventQueueLength, sizeof(AppEvent));
  uiCommandQueue = xQueueCreate(config::kUiCommandQueueLength, sizeof(UiCommand));
  scaleCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(ScaleCommand));
  nfcCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(NfcCommand));
  storageCommandQueue =
      xQueueCreate(config::kStorageCommandQueueLength, sizeof(StorageCommand));
  networkCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(NetworkCommand));
  // Both queues must be empty when they are added to a FreeRTOS queue set.
  // Create and connect them before any producer task can enqueue the network
  // configuration loaded from storage.
  wifiEventQueue =
      xQueueCreate(config::kWifiEventQueueLength, sizeof(std::uint8_t));
  networkQueueSet = xQueueCreateSet(config::kServiceCommandQueueLength +
                                    config::kWifiEventQueueLength);
  const bool networkQueuesReady =
      networkCommandQueue != nullptr && wifiEventQueue != nullptr &&
      networkQueueSet != nullptr &&
      xQueueAddToSet(networkCommandQueue, networkQueueSet) == pdPASS &&
      xQueueAddToSet(wifiEventQueue, networkQueueSet) == pdPASS;
  spoolmanCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(SpoolmanCommand));
  bambuCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(BambuCommand));
  logQueue = xQueueCreate(config::kLogQueueLength, sizeof(LogMessage));
  systemEventGroup = xEventGroupCreate();

  return appEventQueue && uiCommandQueue && scaleCommandQueue && nfcCommandQueue &&
         storageCommandQueue && networkCommandQueue && networkQueuesReady &&
         spoolmanCommandQueue &&
         bambuCommandQueue && logQueue && systemEventGroup;
}

bool RtosContext::createUiTask() {
  if (loggingTask == nullptr &&
      !createTask(loggingTaskMain, config::kLoggingTask, this, &loggingTask)) {
    return false;
  }
  return createTask(tasks::uiTask, config::kUiTask, this, &uiTask);
}

bool RtosContext::createServiceTasks() {
  // Storage wird zuerst angelegt, danach die zentrale Anwendungssteuerung.
  return createTask(tasks::storageTask, config::kStorageTask, this, &storageTask) &&
         createTask(tasks::appTask, config::kAppTask, this, &appTask) &&
         createTask(tasks::scaleTask, config::kScaleTask, this, &scaleTask) &&
         createTask(tasks::nfcTask, config::kNfcTask, this, &nfcTask) &&
         createTask(tasks::networkTask, config::kNetworkTask, this, &networkTask) &&
         createTask(tasks::spoolmanTask, config::kSpoolmanTask, this, &spoolmanTask) &&
         createTask(tasks::bambuTask, config::kBambuTask, this, &bambuTask);
}

bool RtosContext::createTasks() {
  return createUiTask() && createServiceTasks();
}

}  // namespace filament_station::rtos
