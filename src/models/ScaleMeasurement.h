/**
 * @file
 * @brief Unused early-scaffolding placeholder; ScaleTask reports
 *        measurements via rtos::AppEvent fields directly instead. No code
 *        references this type.
 */
#pragma once

#include <cstdint>

namespace filament_station::models {
/// @deprecated Unused; ScaleTask reports measurements via rtos::AppEvent fields directly.
struct ScaleMeasurement {
  std::uint32_t requestId;  ///< Unused.
  float weightGrams;        ///< Unused.
  bool stable;               ///< Unused.
};
}

