/**
 * @file
 * @brief Stateless HX711 raw-counts <-> grams conversion math shared by
 *        calibration and measurement.
 */
#pragma once

#include <cstdint>

namespace filament_station {
namespace services {

/// @brief Computes a calibration factor from a known reference weight.
/// @param measuredCounts Raw HX711 reading with the reference weight applied.
/// @param offsetCounts Tare (zero-weight) offset.
/// @param referenceWeightGrams Known weight of the reference object, in grams.
/// @param factorCountsPerGram Out parameter receiving the computed factor.
/// @return false if the inputs are degenerate (zero delta, non-finite/non-positive reference weight, non-finite/zero result).
bool calculateScaleFactor(std::int32_t measuredCounts,
                          std::int32_t offsetCounts,
                          float referenceWeightGrams,
                          float& factorCountsPerGram);

/// @brief Converts a raw HX711 reading to grams using a calibration factor.
/// @param measuredCounts Raw HX711 reading.
/// @param offsetCounts Tare (zero-weight) offset.
/// @param factorCountsPerGram Calibration factor from calculateScaleFactor().
/// @return Weight in grams, or 0.0F if `factorCountsPerGram` is non-finite or zero.
float countsToGrams(std::int32_t measuredCounts, std::int32_t offsetCounts,
                    float factorCountsPerGram);

}  // namespace services
}  // namespace filament_station
