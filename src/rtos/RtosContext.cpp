/**
 * @file
 * @brief Implements rtos::RtosContext: object/task creation and the
 *        log-queue transport.
 */
#include "rtos/RtosContext.h"

#include <cstdio>
#include <cstring>
#include "config/NetworkConfig.h"
#include "config/TaskConfig.h"
#include "rtos/Messages.h"
#include "services/Logger.h"
#include "tasks/Tasks.h"

namespace filament_station::rtos {
namespace {
RtosContext instance;  ///< Backing storage for context().
static_assert(sizeof(LogMessage::text) == config::kLogMessageCapacity);
std::atomic<std::uint32_t> droppedLogLines{0};  ///< Backing storage for droppedLogLineCount().

/// @brief Creates and pins one FreeRTOS task from a config::TaskSettings.
/// @param function Task entry point.
/// @param settings Name, stack size, priority and core affinity to apply.
/// @param taskContext RtosContext pointer passed to `function` as its parameter.
/// @param handle Out parameter receiving the created task handle.
/// @return true if the task was created successfully.
bool createTask(TaskFunction_t function, const config::TaskSettings& settings,
                RtosContext* taskContext, TaskHandle_t* handle) {
  return xTaskCreatePinnedToCore(function, settings.name, settings.stackSize,
                                 taskContext, settings.priority, handle,
                                 settings.core) == pdPASS;
}

}  // namespace

RtosContext& context() { return instance; }

void enqueueLogLine(const char* message) {
  auto& ctx = context();
  if (message == nullptr || ctx.logQueue == nullptr) return;
  LogMessage entry{};
  std::snprintf(entry.text, sizeof(entry.text), "%s", message);
  // Eine volle Queue verwirft nur die komplette neue Zeile. Bereits laufende
  // USB-Ausgaben werden niemals von einem anderen Task unterbrochen.
  if (xQueueSend(ctx.logQueue, &entry, pdMS_TO_TICKS(10)) != pdPASS) {
    droppedLogLines.fetch_add(1, std::memory_order_relaxed);
  }
}

std::uint32_t droppedLogLineCount() {
  return droppedLogLines.load(std::memory_order_relaxed);
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
  powerCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(PowerCommand));
  updateCommandQueue = xQueueCreate(config::kServiceCommandQueueLength, sizeof(UpdateCommand));
  logQueue = xQueueCreate(config::kLogQueueLength, sizeof(LogMessage));
  systemEventGroup = xEventGroupCreate();

  return appEventQueue && uiCommandQueue && scaleCommandQueue && nfcCommandQueue &&
         storageCommandQueue && networkCommandQueue && networkQueuesReady &&
         spoolmanCommandQueue &&
         bambuCommandQueue && powerCommandQueue && updateCommandQueue && logQueue &&
         systemEventGroup;
}

bool RtosContext::createUiTask() {
  if (loggingTask == nullptr &&
      !createTask(services::Logger::task, config::kLoggingTask, this,
                  &loggingTask)) {
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
         createTask(tasks::bambuTask, config::kBambuTask, this, &bambuTask) &&
         createTask(tasks::powerTask, config::kPowerTask, this, &powerTask) &&
         createTask(tasks::updateTask, config::kUpdateTask, this, &updateTask);
}

bool RtosContext::createTasks() {
  return createUiTask() && createServiceTasks();
}

}  // namespace filament_station::rtos
