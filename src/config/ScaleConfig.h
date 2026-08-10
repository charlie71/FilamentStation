#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station::config {

// Raw-count settings that must be validated on the actual load cell.
constexpr std::size_t kScaleMovingAverageWindow = 5;
constexpr float kScaleLowPassAlpha = 0.25F;
constexpr std::int32_t kScaleOutlierThresholdCounts = 50000;
constexpr std::uint8_t kScaleOutlierConfirmationSamples = 3;
constexpr std::int32_t kScaleNegativeSmallThresholdCounts = 1000;
constexpr std::int32_t kScaleStabilityToleranceCounts = 2500;
constexpr std::uint32_t kScaleStabilityTimeMs = 1500;
// The HX711 may produce substantially more samples than the display needs.
// State transitions bypass this limit; continuous weight updates do not.
constexpr std::uint32_t kScaleUiUpdateIntervalMs = 200;

}  // namespace filament_station::config
