/**
 * @file
 * @brief Unused early-scaffolding placeholder; superseded by
 *        models::SpoolmanSpool. No code references this type.
 */
#pragma once

#include <cstdint>

namespace filament_station::models {
/// @deprecated Unused; see models::SpoolmanSpool for the type actually used at runtime.
struct Spool {
  std::uint32_t id;             ///< Unused.
  float remainingWeightGrams;   ///< Unused.
};
}

