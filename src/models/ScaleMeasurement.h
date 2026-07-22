#pragma once

#include <cstdint>

namespace filament_station::models {
struct ScaleMeasurement {
  std::uint32_t requestId;
  float weightGrams;
  bool stable;
};
}

