#pragma once

#include <cstdint>

namespace filament_station {
namespace services {

bool calculateScaleFactor(std::int32_t measuredCounts,
                          std::int32_t offsetCounts,
                          float referenceWeightGrams,
                          float& factorCountsPerGram);

float countsToGrams(std::int32_t measuredCounts, std::int32_t offsetCounts,
                    float factorCountsPerGram);

}  // namespace services
}  // namespace filament_station
