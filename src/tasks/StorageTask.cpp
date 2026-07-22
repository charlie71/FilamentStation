#include "tasks/Tasks.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
namespace filament_station::tasks {
void storageTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::StorageCommand command{};
  for (;;) { xQueueReceive(ctx.storageCommandQueue, &command, portMAX_DELAY); }
}
}  // namespace filament_station::tasks

