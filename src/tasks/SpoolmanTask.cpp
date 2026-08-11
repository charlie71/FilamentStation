#include "tasks/Tasks.h"
#include <cstdio>
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
namespace filament_station::tasks {
void spoolmanTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::SpoolmanCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.spoolmanCommandQueue, &command, portMAX_DELAY) !=
        pdPASS)
      continue;
    if (command.type == rtos::SpoolmanCommandType::ImportTagDefinition) {
      rtos::AppEvent event{};
      event.type = rtos::AppEventType::SpoolmanError;
      event.requestId = command.requestId;
      std::snprintf(event.text, sizeof(event.text),
                    "Spoolman import requires the Spoolman API phase");
      if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
        rtos::logLine("SpoolmanTask: import response queue overflow");
    }
  }
}
}  // namespace filament_station::tasks
