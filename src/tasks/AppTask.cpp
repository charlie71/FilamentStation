#include "tasks/Tasks.h"

#include <cstdio>
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
void appTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::AppEvent event{};
  for (;;) {
    if (xQueueReceive(ctx.appEventQueue, &event, portMAX_DELAY) == pdTRUE &&
        event.type == rtos::AppEventType::UiCommunicationTest) {
      rtos::UiCommand response{};
      response.type = rtos::UiCommandType::CommunicationTestResponse;
      response.requestId = event.requestId;
      std::snprintf(response.title, sizeof(response.title), "RTOS test");
      std::snprintf(response.text, sizeof(response.text), "AppTask acknowledged event");
      if (xQueueSend(ctx.uiCommandQueue, &response, pdMS_TO_TICKS(1000)) != pdPASS) {
        rtos::logLine("AppTask: uiCommandQueue timeout/overflow");
      } else {
        rtos::logLine("AppTask: response sent");
      }
    }
  }
}
}  // namespace filament_station::tasks

