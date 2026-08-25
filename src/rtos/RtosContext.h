#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include "config/TaskConfig.h"

namespace filament_station::rtos {

struct LogMessage {
  char text[config::kLogMessageCapacity]{};
};

struct RtosContext {
  QueueHandle_t appEventQueue = nullptr;
  QueueHandle_t uiCommandQueue = nullptr;
  QueueHandle_t scaleCommandQueue = nullptr;
  QueueHandle_t nfcCommandQueue = nullptr;
  QueueHandle_t storageCommandQueue = nullptr;
  QueueHandle_t networkCommandQueue = nullptr;
  QueueHandle_t wifiEventQueue = nullptr;
  QueueSetHandle_t networkQueueSet = nullptr;
  QueueHandle_t spoolmanCommandQueue = nullptr;
  QueueHandle_t bambuCommandQueue = nullptr;
  QueueHandle_t powerCommandQueue = nullptr;
  QueueHandle_t logQueue = nullptr;
  EventGroupHandle_t systemEventGroup = nullptr;
  TaskHandle_t loggingTask = nullptr;
  TaskHandle_t uiTask = nullptr;
  TaskHandle_t appTask = nullptr;
  TaskHandle_t scaleTask = nullptr;
  TaskHandle_t nfcTask = nullptr;
  TaskHandle_t storageTask = nullptr;
  TaskHandle_t networkTask = nullptr;
  TaskHandle_t spoolmanTask = nullptr;
  TaskHandle_t bambuTask = nullptr;
  TaskHandle_t powerTask = nullptr;

  bool createObjects();
  bool createUiTask();
  bool createServiceTasks();
  bool createTasks();
};

RtosContext& context();
// Low-level transport used exclusively by services::Logger. Application code
// must use FS_LOGE/W/I/D/T so every line has canonical metadata.
void enqueueLogLine(const char* message);
// A full logQueue silently drops the new line (see enqueueLogLine()) rather
// than blocking the producer task -- correct for a long-running device, but
// previously left with zero visibility if it ever actually happened
// (Robustheit/Diagnose, TASKS.md 10.7). Called from every task via
// FS_LOG*, so a lock-free atomic counter instead of a queue/mutex.
// Diagnostics-only; not reset except at boot.
std::uint32_t droppedLogLineCount();

}  // namespace filament_station::rtos
