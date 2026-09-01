/**
 * @file
 * @brief Implements services::ScaleFilter.
 */
#include "services/ScaleFilter.h"

#include <algorithm>

namespace filament_station {
namespace services {

constexpr std::size_t ScaleFilter::kMaximumMovingAverageWindow;

namespace {

/// @brief Absolute difference between two raw-counts values.
/// @param left First value.
/// @param right Second value.
/// @return |left - right|, computed without overflow via 64-bit intermediates.
std::int64_t absoluteDifference(std::int32_t left, std::int32_t right) {
  const std::int64_t difference =
      static_cast<std::int64_t>(left) - static_cast<std::int64_t>(right);
  return difference < 0 ? -difference : difference;
}

/// @brief Rounds a float to the nearest integer (away from zero on .5).
/// @param value Value to round.
/// @return Rounded integer value.
std::int32_t roundToInt(float value) {
  return static_cast<std::int32_t>(value >= 0.0F ? value + 0.5F
                                                 : value - 0.5F);
}

}  // namespace

ScaleFilter::ScaleFilter(const ScaleFilterConfig& config) : config_(config) {
  config_.movingAverageWindow =
      std::min(std::max(config_.movingAverageWindow, std::size_t{1}),
               kMaximumMovingAverageWindow);
  config_.lowPassAlpha = std::min(std::max(config_.lowPassAlpha, 0.0F), 1.0F);
  config_.outlierConfirmationSamples =
      std::max<std::uint8_t>(config_.outlierConfirmationSamples, 1);
  config_.outlierThreshold = std::max(config_.outlierThreshold, 0);
  config_.negativeSmallThreshold =
      std::max(config_.negativeSmallThreshold, 0);
  config_.stabilityTolerance = std::max(config_.stabilityTolerance, 0);
}

void ScaleFilter::reset() {
  sampleCount_ = 0;
  sampleIndex_ = 0;
  sampleSum_ = 0;
  hasAcceptedValue_ = false;
  lastAcceptedValue_ = 0;
  hasOutlierCandidate_ = false;
  outlierCandidate_ = 0;
  outlierCandidateCount_ = 0;
  lowPassInitialized_ = false;
  lowPassValue_ = 0.0F;
  stabilityInitialized_ = false;
  stabilityReference_ = 0;
  stabilitySinceMs_ = 0;
  stable_ = false;
  samples_.fill(0);
}

std::int32_t ScaleFilter::clampNegativeSmallValue(std::int32_t value) const {
  return value < 0 &&
                 static_cast<std::int64_t>(value) >=
                     -static_cast<std::int64_t>(config_.negativeSmallThreshold)
             ? 0
             : value;
}

bool ScaleFilter::acceptCandidate(std::int32_t value) {
  if (!hasAcceptedValue_ ||
      absoluteDifference(value, lastAcceptedValue_) <=
          config_.outlierThreshold) {
    hasOutlierCandidate_ = false;
    outlierCandidateCount_ = 0;
    lastAcceptedValue_ = value;
    hasAcceptedValue_ = true;
    return true;
  }

  const std::int32_t candidateTolerance =
      std::max<std::int32_t>(config_.outlierThreshold / 4, 1);
  if (!hasOutlierCandidate_ ||
      absoluteDifference(value, outlierCandidate_) > candidateTolerance) {
    hasOutlierCandidate_ = true;
    outlierCandidate_ = value;
    outlierCandidateCount_ = 1;
    if (config_.outlierConfirmationSamples == 1) {
      lastAcceptedValue_ = value;
      hasOutlierCandidate_ = false;
      outlierCandidateCount_ = 0;
      return true;
    }
    return false;
  }

  if (outlierCandidateCount_ < UINT8_MAX) ++outlierCandidateCount_;
  if (outlierCandidateCount_ < config_.outlierConfirmationSamples) return false;

  lastAcceptedValue_ = value;
  hasOutlierCandidate_ = false;
  outlierCandidateCount_ = 0;
  return true;
}

std::int32_t ScaleFilter::updateMovingAverage(std::int32_t value) {
  if (sampleCount_ < config_.movingAverageWindow) {
    samples_[sampleIndex_] = value;
    sampleSum_ += value;
    ++sampleCount_;
  } else {
    sampleSum_ -= samples_[sampleIndex_];
    samples_[sampleIndex_] = value;
    sampleSum_ += value;
  }
  sampleIndex_ = (sampleIndex_ + 1U) % config_.movingAverageWindow;
  return static_cast<std::int32_t>(sampleSum_ /
                                   static_cast<std::int64_t>(sampleCount_));
}

std::int32_t ScaleFilter::updateLowPass(std::int32_t value) {
  if (!lowPassInitialized_) {
    lowPassValue_ = static_cast<float>(value);
    lowPassInitialized_ = true;
  } else {
    lowPassValue_ +=
        config_.lowPassAlpha * (static_cast<float>(value) - lowPassValue_);
  }
  return roundToInt(lowPassValue_);
}

ScaleFilterResult ScaleFilter::updateStability(std::int32_t value,
                                                std::uint32_t timestampMs,
                                                bool outlierRejected) {
  ScaleFilterResult result{value, stable_, false, outlierRejected};
  if (!stabilityInitialized_) {
    stabilityInitialized_ = true;
    stabilityReference_ = value;
    stabilitySinceMs_ = timestampMs;
    return result;
  }

  if (absoluteDifference(value, stabilityReference_) >
      config_.stabilityTolerance) {
    stabilityReference_ = value;
    stabilitySinceMs_ = timestampMs;
    if (stable_) {
      stable_ = false;
      result.stabilityChanged = true;
    }
  } else if (!stable_ &&
             timestampMs - stabilitySinceMs_ >= config_.stabilityTimeMs) {
    stable_ = true;
    result.stabilityChanged = true;
  }
  result.stable = stable_;
  return result;
}

ScaleFilterResult ScaleFilter::process(std::int32_t rawCounts,
                                       std::uint32_t timestampMs) {
  const std::int32_t normalized = clampNegativeSmallValue(rawCounts);
  const bool accepted = acceptCandidate(normalized);
  if (!accepted) {
    const std::int32_t current =
        clampNegativeSmallValue(roundToInt(lowPassValue_));
    return updateStability(current, timestampMs, true);
  }

  const std::int32_t averaged = updateMovingAverage(normalized);
  const std::int32_t filtered =
      clampNegativeSmallValue(updateLowPass(averaged));
  return updateStability(filtered, timestampMs, false);
}

}  // namespace services
}  // namespace filament_station
