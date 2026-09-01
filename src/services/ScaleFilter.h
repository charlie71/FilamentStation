/**
 * @file
 * @brief Stateful HX711 raw-reading filter: outlier rejection, moving
 *        average, low-pass smoothing, and stability detection.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace services {

/// @brief Tunable parameters for ScaleFilter.
struct ScaleFilterConfig {
  std::size_t movingAverageWindow;         ///< Number of samples averaged, clamped to ScaleFilter::kMaximumMovingAverageWindow.
  float lowPassAlpha;                      ///< Low-pass smoothing factor in [0,1]; higher tracks new values faster.
  std::int32_t outlierThreshold;           ///< Maximum accepted jump (in raw counts) from the last accepted value before a sample is treated as a possible outlier.
  std::uint8_t outlierConfirmationSamples; ///< Consecutive similar samples required to accept a jump beyond #outlierThreshold as real.
  std::int32_t negativeSmallThreshold;     ///< Small negative readings within this many counts of zero are clamped to 0.
  std::int32_t stabilityTolerance;         ///< Maximum drift (in raw counts) from the stability reference still considered stable.
  std::uint32_t stabilityTimeMs;           ///< Duration a reading must stay within #stabilityTolerance before being reported stable.
};

/// @brief Output of one ScaleFilter::process() call.
struct ScaleFilterResult {
  std::int32_t value = 0;      ///< Filtered raw-counts value.
  bool stable = false;         ///< Whether the reading is currently considered stable.
  bool stabilityChanged = false;  ///< Whether #stable flipped on this call.
  bool outlierRejected = false;   ///< Whether this sample was rejected as an unconfirmed outlier.
};

/// @brief Applies outlier rejection, moving-average and low-pass smoothing,
///        and stability detection to a stream of raw HX711 readings.
class ScaleFilter {
 public:
  static constexpr std::size_t kMaximumMovingAverageWindow = 16;  ///< Upper bound on ScaleFilterConfig::movingAverageWindow.

  /// @brief Constructs the filter, clamping out-of-range config values.
  /// @param config Filter tuning parameters.
  explicit ScaleFilter(const ScaleFilterConfig& config);
  /// @brief Feeds one raw HX711 reading through the filter pipeline.
  /// @param rawCounts Raw HX711 sample.
  /// @param timestampMs Sample timestamp in milliseconds, used for stability timing.
  /// @return The filtered value plus stability/outlier status.
  ScaleFilterResult process(std::int32_t rawCounts, std::uint32_t timestampMs);
  /// @brief Clears all filter state (moving average, low-pass, stability, outlier tracking).
  void reset();

 private:
  /// @brief Clamps small negative readings near zero to exactly 0.
  /// @param value Raw or filtered value to clamp.
  /// @return `value`, or 0 if it is a small negative reading per ScaleFilterConfig::negativeSmallThreshold.
  std::int32_t clampNegativeSmallValue(std::int32_t value) const;
  /// @brief Decides whether a normalized sample should be accepted as real
  ///        or held as an unconfirmed outlier candidate.
  /// @param value Normalized sample value.
  /// @return true if the sample is accepted into the filter pipeline.
  bool acceptCandidate(std::int32_t value);
  /// @brief Updates the moving-average window with a new accepted value.
  /// @param value Accepted sample value.
  /// @return Current moving-average value.
  std::int32_t updateMovingAverage(std::int32_t value);
  /// @brief Updates the low-pass filter with a new moving-average value.
  /// @param value Moving-average value.
  /// @return Current low-pass filtered value, rounded to the nearest integer.
  std::int32_t updateLowPass(std::int32_t value);
  /// @brief Updates stability tracking with a new filtered value.
  /// @param value Filtered value.
  /// @param timestampMs Sample timestamp in milliseconds.
  /// @param outlierRejected Whether the underlying sample was an outlier rejection.
  /// @return Result reflecting the updated stability state.
  ScaleFilterResult updateStability(std::int32_t value,
                                    std::uint32_t timestampMs,
                                    bool outlierRejected);

  ScaleFilterConfig config_;    ///< Clamped filter configuration.
  std::array<std::int32_t, kMaximumMovingAverageWindow> samples_{};  ///< Ring buffer backing the moving average.
  std::size_t sampleCount_ = 0;   ///< Number of valid entries currently in #samples_.
  std::size_t sampleIndex_ = 0;   ///< Next write position in #samples_.
  std::int64_t sampleSum_ = 0;    ///< Running sum of #samples_, for O(1) average computation.
  bool hasAcceptedValue_ = false; ///< Whether #lastAcceptedValue_ holds a real prior sample.
  std::int32_t lastAcceptedValue_ = 0;  ///< Most recently accepted normalized sample.
  bool hasOutlierCandidate_ = false;    ///< Whether an unconfirmed outlier candidate is pending.
  std::int32_t outlierCandidate_ = 0;   ///< Value of the pending outlier candidate.
  std::uint8_t outlierCandidateCount_ = 0;  ///< Consecutive similar samples seen for the pending candidate.
  bool lowPassInitialized_ = false;  ///< Whether #lowPassValue_ has been seeded yet.
  float lowPassValue_ = 0.0F;        ///< Current low-pass filtered value.
  bool stabilityInitialized_ = false;  ///< Whether #stabilityReference_ has been seeded yet.
  std::int32_t stabilityReference_ = 0;  ///< Value stability drift is measured against.
  std::uint32_t stabilitySinceMs_ = 0;   ///< Timestamp #stabilityReference_ was last reset.
  bool stable_ = false;  ///< Current reported stability state.
};

}  // namespace services
}  // namespace filament_station
