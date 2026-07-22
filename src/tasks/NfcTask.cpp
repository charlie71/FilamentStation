#include "tasks/Tasks.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
namespace filament_station::tasks {
void nfcTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::NfcCommand command{};
  for (;;) { xQueueReceive(ctx.nfcCommandQueue, &command, portMAX_DELAY); }
}
}  // namespace filament_station::tasks

