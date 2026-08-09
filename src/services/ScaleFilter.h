#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace filament_station::services {

struct ScaleFilterConfig {
  std::size_t movingAverageWindow;
  float lowPassAlpha;
  std::int32_t outlierThreshold;
  std::uint8_t outlierConfirmationSamples;
  std::int32_t negativeSmallThreshold;
  std::int32_t stabilityTolerance;
  std::uint32_t stabilityTimeMs;
};

struct ScaleFilterResult {
  std::int32_t value = 0;
  bool stable = false;
  bool stabilityChanged = false;
  bool outlierRejected = false;
};

class ScaleFilter {
 public:
  static constexpr std::size_t kMaximumMovingAverageWindow = 16;

  explicit ScaleFilter(const ScaleFilterConfig& config);
  ScaleFilterResult process(std::int32_t rawCounts, std::uint32_t timestampMs);
  void reset();

 private:
  std::int32_t clampNegativeSmallValue(std::int32_t value) const;
  bool acceptCandidate(std::int32_t value);
  std::int32_t updateMovingAverage(std::int32_t value);
  std::int32_t updateLowPass(std::int32_t value);
  ScaleFilterResult updateStability(std::int32_t value,
                                    std::uint32_t timestampMs,
                                    bool outlierRejected);

  ScaleFilterConfig config_;
  std::array<std::int32_t, kMaximumMovingAverageWindow> samples_{};
  std::size_t sampleCount_ = 0;
  std::size_t sampleIndex_ = 0;
  std::int64_t sampleSum_ = 0;
  bool hasAcceptedValue_ = false;
  std::int32_t lastAcceptedValue_ = 0;
  bool hasOutlierCandidate_ = false;
  std::int32_t outlierCandidate_ = 0;
  std::uint8_t outlierCandidateCount_ = 0;
  bool lowPassInitialized_ = false;
  float lowPassValue_ = 0.0F;
  bool stabilityInitialized_ = false;
  std::int32_t stabilityReference_ = 0;
  std::uint32_t stabilitySinceMs_ = 0;
  bool stable_ = false;
};

}  // namespace filament_station::services
