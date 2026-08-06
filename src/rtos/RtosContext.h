#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace filament_station::rtos {

struct RtosContext {
  QueueHandle_t appEventQueue = nullptr;
  QueueHandle_t uiCommandQueue = nullptr;
  QueueHandle_t scaleCommandQueue = nullptr;
  QueueHandle_t nfcCommandQueue = nullptr;
  QueueHandle_t storageCommandQueue = nullptr;
  QueueHandle_t networkCommandQueue = nullptr;
  QueueHandle_t spoolmanCommandQueue = nullptr;
  QueueHandle_t bambuCommandQueue = nullptr;
  EventGroupHandle_t systemEventGroup = nullptr;
  SemaphoreHandle_t debugMutex = nullptr;
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

}  // namespace filament_station::rtos
