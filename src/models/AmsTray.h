/**
 * @file
 * @brief Unused early-scaffolding placeholder; superseded by
 *        models::PrinterSlotStateData/models::TraySpoolCacheEntry. No code
 *        references this type.
 */
#pragma once

#include <cstdint>

namespace filament_station::models {
/// @deprecated Unused; see models::PrinterSlotStateData and models::TraySpoolCacheEntry for the types actually used at runtime.
struct AmsTray {
  std::uint8_t trayId;   ///< Unused.
  std::uint32_t spoolId;  ///< Unused.
};
}

