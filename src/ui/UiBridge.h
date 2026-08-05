#pragma once

#include <cstddef>
#include <cstdint>

#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::ui {

struct UiRuntimeInfo {
  std::size_t bytesPerDrawBuffer = 0;
  std::size_t totalDrawBufferBytes = 0;
  bool drawBuffersInPsram = false;
};

bool initializeLvgl(UiRuntimeInfo& runtimeInfo, rtos::RtosContext& context);
std::uint32_t runLvglTimers();
void processUiCommand(const rtos::UiCommand& command);

}  // namespace filament_station::ui
