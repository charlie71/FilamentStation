#include "tasks/Tasks.h"

#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"

namespace filament_station::tasks {

void bambuTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::BambuCommand command{};

  for (;;) {
    if (xQueueReceive(ctx.bambuCommandQueue, &command, portMAX_DELAY) !=
        pdTRUE) {
      continue;
    }

    // Phase-1-Taskgeruest: Befehle werden bewusst nur entgegengenommen.
    // MQTT, Netzwerkzugriff und reale Druckerlogik folgen in der Bambu-Phase.
    FS_LOGD(services::LogComponent::Bambu,
            "Command received status=implementation_pending");
  }
}

}  // namespace filament_station::tasks
