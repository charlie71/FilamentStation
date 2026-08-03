#include "tasks/Tasks.h"

#include <cstdio>
#include "config/AppConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
void uiTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
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
  for (;;) {
    if (xQueueReceive(ctx.uiCommandQueue, &command, portMAX_DELAY) == pdTRUE) {
      char line[128];
      std::snprintf(line, sizeof(line), "UiTask: response received (requestId=%lu): %s",
                    static_cast<unsigned long>(command.requestId), command.text);
      rtos::logLine(line);
    }
  }
}
}  // namespace filament_station::tasks
