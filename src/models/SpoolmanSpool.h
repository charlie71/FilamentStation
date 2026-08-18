#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

struct SpoolmanSpool {
  static constexpr std::uint8_t kMaximumColors = 3;
  std::uint32_t id = 0;
  char vendor[32]{};
  char filament[40]{};
  char material[24]{};
  char colorHex[kMaximumColors][9]{};
  std::uint8_t colorCount = 0;
  float initialWeightGrams = 0.0F;
  float emptyWeightGrams = 0.0F;
  float remainingWeightGrams = 0.0F;
  bool archived = false;
};

}  // namespace models
}  // namespace filament_station
