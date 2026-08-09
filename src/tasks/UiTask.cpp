#include "tasks/Tasks.h"

#include <Arduino.h>
#include <cstdio>
#include <limits>
#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "drivers/DisplayDriver.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "ui/UiBridge.h"

namespace filament_station::tasks {
void uiTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  if (!drivers::initializeDisplay()) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    rtos::logLine("UiTask: LovyanGFX display initialization failed");
    vTaskSuspend(nullptr);
  }
  rtos::logLine("UiTask: LovyanGFX display ready (480x320, rotation 3)");

  const std::uint32_t psramBefore = ESP.getFreePsram();
  ui::UiRuntimeInfo runtimeInfo{};
  if (!ui::initializeLvgl(runtimeInfo, ctx)) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    rtos::logLine("UiTask: LVGL initialization or PSRAM allocation failed");
    vTaskSuspend(nullptr);
  }
  const std::uint32_t psramAfter = ESP.getFreePsram();
  const std::uint32_t psramConsumed =
      psramBefore >= psramAfter ? psramBefore - psramAfter : 0;
  char lvglLine[128];
  std::snprintf(lvglLine, sizeof(lvglLine),
                "UiTask: LVGL ready; buffers=%u x 2; PSRAM used=%lu free=%lu",
                static_cast<unsigned int>(runtimeInfo.bytesPerDrawBuffer),
                static_cast<unsigned long>(psramConsumed),
                static_cast<unsigned long>(psramAfter));
  rtos::logLine(lvglLine);
  if (!runtimeInfo.drawBuffersInPsram) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    rtos::logLine("UiTask: LVGL draw buffers are not fully backed by PSRAM");
    vTaskSuspend(nullptr);
  }

  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_UI_READY);

  rtos::AppEvent event{};
  event.type = rtos::AppEventType::UiCommunicationTest;
  event.requestId = config::kCommunicationTestRequestId;
  std::snprintf(event.text, sizeof(event.text),
                "UiTask communication test");
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    rtos::logLine("UiTask: appEventQueue timeout/overflow");
  } else {
    rtos::logLine("UiTask: test event sent");
  }

  rtos::UiCommand command{};
  for (;;) {
    const std::uint32_t requestedSleepMs = ui::runLvglTimers();
    const std::uint32_t sleepMs =
        requestedSleepMs < config::kLvglMinimumSleepMs
            ? config::kLvglMinimumSleepMs
            : requestedSleepMs;
    const TickType_t waitTicks =
        sleepMs == std::numeric_limits<std::uint32_t>::max()
            ? portMAX_DELAY
            : pdMS_TO_TICKS(sleepMs);
    if (xQueueReceive(ctx.uiCommandQueue, &command,
                      waitTicks) == pdTRUE) {
      do {
        ui::processUiCommand(command);
        char line[128];
        std::snprintf(
            line, sizeof(line),
            "UiTask: response received (requestId=%lu): %s",
            static_cast<unsigned long>(command.requestId), command.text);
        rtos::logLine(line);
      } while (xQueueReceive(ctx.uiCommandQueue, &command, 0) == pdTRUE);
    }
  }
}
}  // namespace filament_station::tasks
