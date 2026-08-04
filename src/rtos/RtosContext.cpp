#include "rtos/RtosContext.h"

#include <Arduino.h>
#include "config/TaskConfig.h"
#include "rtos/Messages.h"
#include "tasks/Tasks.h"

namespace filament_station::rtos {
namespace {
RtosContext instance;

bool createTask(TaskFunction_t function, const config::TaskSettings& settings,
                RtosContext* taskContext, TaskHandle_t* handle) {
  return xTaskCreatePinnedToCore(function, settings.name, settings.stackSize,
                                 taskContext, settings.priority, handle,
                                 settings.core) == pdPASS;
}
}  // namespace

RtosContext& context() { return instance; }

void logLine(const char* message) {
  auto& ctx = context();
  if (ctx.debugMutex != nullptr && xSemaphoreTake(ctx.debugMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println(message);
    xSemaphoreGive(ctx.debugMutex);
  }
}

bool RtosContext::createObjects() {
  appEventQueue = xQueueCreate(config::kAppEventQueueLength, sizeof(AppEvent));
  uiCommandQueue = xQueueCreate(config::kUiCommandQueueLength, sizeof(UiCommand));
  scaleCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(ScaleCommand));
  nfcCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(NfcCommand));
  storageCommandQueue =
      xQueueCreate(config::kStorageCommandQueueLength, sizeof(StorageCommand));
  networkCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(NetworkCommand));
  spoolmanCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(SpoolmanCommand));
  bambuCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(BambuCommand));
  systemEventGroup = xEventGroupCreate();
  debugMutex = xSemaphoreCreateMutex();

  return appEventQueue && uiCommandQueue && scaleCommandQueue && nfcCommandQueue &&
         storageCommandQueue && networkCommandQueue && spoolmanCommandQueue &&
         bambuCommandQueue && systemEventGroup && debugMutex;
}

bool RtosContext::createTasks() {
  // Storage wird zuerst angelegt, danach die zentrale Anwendungssteuerung.
  return createTask(tasks::storageTask, config::kStorageTask, this, &storageTask) &&
         createTask(tasks::appTask, config::kAppTask, this, &appTask) &&
         createTask(tasks::uiTask, config::kUiTask, this, &uiTask) &&
         createTask(tasks::scaleTask, config::kScaleTask, this, &scaleTask) &&
         createTask(tasks::nfcTask, config::kNfcTask, this, &nfcTask) &&
         createTask(tasks::networkTask, config::kNetworkTask, this, &networkTask) &&
         createTask(tasks::spoolmanTask, config::kSpoolmanTask, this, &spoolmanTask);
}

}  // namespace filament_station::rtos
