/**
 * @file
 * @brief Filtering and stability-detection tuning constants for the HX711
 *        weighing pipeline (services::ScaleFilter, services::ScaleMath).
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station::config {

// Raw-count settings that must be validated on the actual load cell.
constexpr std::size_t kScaleMovingAverageWindow = 5;              ///< Number of raw samples averaged into one filtered reading.
constexpr float kScaleLowPassAlpha = 0.25F;                       ///< Exponential low-pass filter coefficient applied on top of the moving average.
constexpr std::int32_t kScaleOutlierThresholdCounts = 50000;      ///< Raw-count jump beyond which a sample is treated as a candidate outlier.
constexpr std::uint8_t kScaleOutlierConfirmationSamples = 3;      ///< Consecutive samples required to confirm a jump as real rather than noise.
constexpr std::int32_t kScaleNegativeSmallThresholdCounts = 1000;  ///< Small negative readings within this bound are clamped to zero (tare drift) instead of reported as negative weight.
constexpr std::int32_t kScaleStabilityToleranceCounts = 2500;     ///< Maximum raw-count spread within the stability window to be considered stable.
constexpr std::uint32_t kScaleStabilityTimeMs = 1500;             ///< Duration the reading must stay within tolerance before being reported as stable.
// The HX711 may produce substantially more samples than the display needs.
// State transitions bypass this limit; continuous weight updates do not.
constexpr std::uint32_t kScaleUiUpdateIntervalMs = 200;  ///< Minimum interval between continuous (non-transition) weight updates sent to the UI.

}  // namespace filament_station::config
