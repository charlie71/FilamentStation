#include "tasks/Tasks.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
namespace filament_station::tasks {
void networkTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::NetworkCommand command{};
  for (;;) { xQueueReceive(ctx.networkCommandQueue, &command, portMAX_DELAY); }
}
}  // namespace filament_station::tasks

