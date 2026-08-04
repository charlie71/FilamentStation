#include "tasks/Tasks.h"

#include <cstdio>
#include <cstdlib>
#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/TouchDriver.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
void uiTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  if (!drivers::initializeDisplay()) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    rtos::logLine("UiTask: LovyanGFX display initialization failed");
    vTaskSuspend(nullptr);
  }
  drivers::drawDisplayColorTest();
  rtos::logLine("UiTask: LovyanGFX display ready (480x320, rotation 3)");

  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_UI_READY);

  const rtos::AppEvent event{rtos::AppEventType::UiCommunicationTest,
                             config::kCommunicationTestRequestId, 0,
                             "UiTask communication test"};
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    rtos::logLine("UiTask: appEventQueue timeout/overflow");
  } else {
    rtos::logLine("UiTask: test event sent");
  }

  rtos::UiCommand command{};
  bool touching = false;
  std::int32_t lastX = -1;
  std::int32_t lastY = -1;
  for (;;) {
    if (xQueueReceive(ctx.uiCommandQueue, &command,
                      pdMS_TO_TICKS(config::kTouchSampleIntervalMs)) == pdTRUE) {
      char line[128];
      std::snprintf(line, sizeof(line), "UiTask: response received (requestId=%lu): %s",
                    static_cast<unsigned long>(command.requestId), command.text);
      rtos::logLine(line);
    }

    std::int32_t x = 0;
    std::int32_t y = 0;
    if (drivers::readTouchCoordinates(x, y)) {
      drivers::displayDevice().fillCircle(x, y, 3, TFT_YELLOW);
      if (!touching || std::abs(x - lastX) >= 8 || std::abs(y - lastY) >= 8) {
        char line[96];
        std::snprintf(line, sizeof(line),
                      "UiTask: touch landscape x=%ld y=%ld",
                      static_cast<long>(x), static_cast<long>(y));
        rtos::logLine(line);
        lastX = x;
        lastY = y;
      }
      touching = true;
    } else if (touching) {
      touching = false;
      rtos::logLine("UiTask: touch released");
    }
  }
}
}  // namespace filament_station::tasks
