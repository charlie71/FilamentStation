#include "tasks/Tasks.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
namespace filament_station::tasks {
void scaleTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::ScaleCommand command{};
  for (;;) { xQueueReceive(ctx.scaleCommandQueue, &command, portMAX_DELAY); }
}
}  // namespace filament_station::tasks

