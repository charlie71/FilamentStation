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
// Milliseconds since the last LVGL input event (any indev, in practice only
// touch here) -- source signal for the power-save statemachine (TASKS.md
// Phase 11). Kept behind this wrapper so LVGL access stays inside UiTask,
// per "Nur UiTask greift auf LVGL zu".
std::uint32_t inputInactiveMs();
void processUiCommand(const rtos::UiCommand& command);

}  // namespace filament_station::ui
