/**
 * @file
 * @brief Unused early-scaffolding placeholder; superseded by
 *        models::TagReadResult/models::TagIdentity. No code references
 *        this type.
 */
#pragma once

#include <cstdint>

namespace filament_station::models {
/// @deprecated Unused; see models::TagReadResult and models::TagIdentity for the types actually used at runtime.
struct NfcTag {
  std::uint8_t uid[10];      ///< Unused.
  std::uint8_t uidLength;    ///< Unused.
  std::uint32_t spoolId;     ///< Unused.
};
}

