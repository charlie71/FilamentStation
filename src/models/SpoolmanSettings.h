#pragma once

#include <cstdint>

namespace filament_station::models {

struct SpoolmanSettings {
  bool enabled = false;
  char name[32]{};
  char serverUrl[128]{};
  std::uint32_t timeoutMs = 5000;
};

}  // namespace filament_station::models
