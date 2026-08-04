#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station::ui {

struct UiRuntimeInfo {
  std::size_t bytesPerDrawBuffer = 0;
  std::size_t totalDrawBufferBytes = 0;
  bool drawBuffersInPsram = false;
};

bool initializeLvgl(UiRuntimeInfo& runtimeInfo);
void runLvglTimers();

}  // namespace filament_station::ui
