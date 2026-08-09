#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station::config {

// Preliminary raw-count settings. Phase 4.4 calibration will establish the
// conversion to grams; these values must then be validated on the load cell.
constexpr std::size_t kScaleMovingAverageWindow = 5;
constexpr float kScaleLowPassAlpha = 0.25F;
constexpr std::int32_t kScaleOutlierThresholdCounts = 50000;
constexpr std::uint8_t kScaleOutlierConfirmationSamples = 3;
constexpr std::int32_t kScaleNegativeSmallThresholdCounts = 1000;
constexpr std::int32_t kScaleStabilityToleranceCounts = 2500;
constexpr std::uint32_t kScaleStabilityTimeMs = 1500;

}  // namespace filament_station::config
