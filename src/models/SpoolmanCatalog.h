#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

struct SpoolmanVendor {
  std::uint32_t id = 0;
  char name[65]{};
  float emptySpoolWeightGrams = 0.0F;
};

struct SpoolmanFilament {
  std::uint32_t id = 0;
  std::uint32_t vendorId = 0;
  char name[65]{};
  char material[65]{};
  char colorHex[9]{};
  float densityGramsPerCm3 = 0.0F;
  float diameterMillimeters = 0.0F;
  float weightGrams = 0.0F;
  float emptySpoolWeightGrams = 0.0F;
  std::int16_t nozzleTemperatureC = 0;
  std::int16_t bedTemperatureC = 0;
};

}  // namespace models
}  // namespace filament_station
