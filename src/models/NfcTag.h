#pragma once

#include <cstdint>

namespace filament_station::models {
struct NfcTag {
  std::uint8_t uid[10];
  std::uint8_t uidLength;
  std::uint32_t spoolId;
};
}

