#include "services/ScaleMath.h"

#include <cmath>

namespace filament_station {
namespace services {

bool calculateScaleFactor(std::int32_t measuredCounts,
                          std::int32_t offsetCounts,
                          float referenceWeightGrams,
                          float& factorCountsPerGram) {
  const std::int64_t delta = static_cast<std::int64_t>(measuredCounts) -
                             static_cast<std::int64_t>(offsetCounts);
  if (delta == 0 || !std::isfinite(referenceWeightGrams) ||
      referenceWeightGrams <= 0.0F) {
    return false;
  }
  factorCountsPerGram = static_cast<float>(delta) / referenceWeightGrams;
  return std::isfinite(factorCountsPerGram) && factorCountsPerGram != 0.0F;
}

float countsToGrams(std::int32_t measuredCounts, std::int32_t offsetCounts,
                    float factorCountsPerGram) {
  if (!std::isfinite(factorCountsPerGram) || factorCountsPerGram == 0.0F)
    return 0.0F;
  const std::int64_t delta = static_cast<std::int64_t>(measuredCounts) -
                             static_cast<std::int64_t>(offsetCounts);
  return static_cast<float>(delta) / factorCountsPerGram;
}

}  // namespace services
}  // namespace filament_station
