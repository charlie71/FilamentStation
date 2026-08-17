#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <cstddef>
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
  QueueHandle_t spoolmanCommandQueue = nullptr;
  QueueHandle_t bambuCommandQueue = nullptr;
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

  bool createObjects();
  bool createUiTask();
  bool createServiceTasks();
  bool createTasks();
};

RtosContext& context();
void logLine(const char* message);
void logf(const char* format, ...);

}  // namespace filament_station::rtos
