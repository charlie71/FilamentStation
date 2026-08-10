#include <unity.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "services/ScaleFilter.h"
#include "services/ScaleMath.h"

void setUp() {}
void tearDown() {}

namespace {

using filament_station::services::ScaleFilter;
using filament_station::services::ScaleFilterConfig;
using filament_station::services::ScaleFilterResult;
using filament_station::services::calculateScaleFactor;
using filament_station::services::countsToGrams;

ScaleFilterConfig config(std::size_t window = 1, float alpha = 1.0F) {
  return {window, alpha, 1000000, 3, 10, 5, 1000};
}

void test_moving_average() {
  ScaleFilter filter(config(3));
  TEST_ASSERT_EQUAL_INT32(10, filter.process(10, 0).value);
  TEST_ASSERT_EQUAL_INT32(15, filter.process(20, 10).value);
  TEST_ASSERT_EQUAL_INT32(20, filter.process(30, 20).value);
  TEST_ASSERT_EQUAL_INT32(30, filter.process(40, 30).value);
}

void test_low_pass() {
  ScaleFilter filter(config(1, 0.5F));
  TEST_ASSERT_EQUAL_INT32(100, filter.process(100, 0).value);
  TEST_ASSERT_EQUAL_INT32(150, filter.process(200, 10).value);
  TEST_ASSERT_EQUAL_INT32(175, filter.process(200, 20).value);
}

void test_outlier_and_confirmed_step() {
  ScaleFilterConfig settings = config();
  settings.outlierThreshold = 50;
  ScaleFilter filter(settings);
  TEST_ASSERT_EQUAL_INT32(1000, filter.process(1000, 0).value);
  TEST_ASSERT_TRUE(filter.process(5000, 10).outlierRejected);
  TEST_ASSERT_EQUAL_INT32(1005, filter.process(1005, 20).value);
  TEST_ASSERT_TRUE(filter.process(2000, 30).outlierRejected);
  TEST_ASSERT_TRUE(filter.process(2002, 40).outlierRejected);
  const auto accepted = filter.process(1998, 50);
  TEST_ASSERT_FALSE(accepted.outlierRejected);
  TEST_ASSERT_EQUAL_INT32(1998, accepted.value);
}

void test_small_negative_values() {
  ScaleFilter filter(config());
  TEST_ASSERT_EQUAL_INT32(0, filter.process(-5, 0).value);
  TEST_ASSERT_EQUAL_INT32(-11, filter.process(-11, 10).value);
}

void test_stability_time_and_reset() {
  ScaleFilter filter(config());
  TEST_ASSERT_FALSE(filter.process(100, 0).stable);
  TEST_ASSERT_FALSE(filter.process(102, 999).stable);
  const auto stable = filter.process(101, 1000);
  TEST_ASSERT_TRUE(stable.stable);
  TEST_ASSERT_TRUE(stable.stabilityChanged);
  const auto unstable = filter.process(120, 1100);
  TEST_ASSERT_FALSE(unstable.stable);
  TEST_ASSERT_TRUE(unstable.stabilityChanged);
}

void test_calibration_calculation() {
  float factor = 0.0F;
  TEST_ASSERT_TRUE(calculateScaleFactor(125000, 25000, 500.0F, factor));
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 200.0F, factor);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 500.0F,
                           countsToGrams(125000, 25000, factor));
  TEST_ASSERT_FALSE(calculateScaleFactor(25000, 25000, 500.0F, factor));
  TEST_ASSERT_FALSE(calculateScaleFactor(125000, 25000, 0.0F, factor));
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F,
                           countsToGrams(125000, 25000, 0.0F));
}

void test_simulated_interrupt_sequence() {
  ScaleFilter filter(config(1));
  struct SimulatedInterrupt {
    std::int32_t rawCounts;
    std::uint32_t timestampMs;
  };
  const SimulatedInterrupt interrupts[]{{1000, 0}, {1002, 400},
                                        {1001, 1000}, {1020, 1100}};
  std::size_t processedNotifications = 0;
  ScaleFilterResult result{};
  for (const auto& interrupt : interrupts) {
    ++processedNotifications;
    result = filter.process(interrupt.rawCounts, interrupt.timestampMs);
  }
  TEST_ASSERT_EQUAL_UINT32(4, processedNotifications);
  TEST_ASSERT_FALSE(result.stable);
  TEST_ASSERT_TRUE(result.stabilityChanged);
  TEST_ASSERT_EQUAL_INT32(1020, result.value);
}

}  // namespace

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_moving_average);
  RUN_TEST(test_low_pass);
  RUN_TEST(test_outlier_and_confirmed_step);
  RUN_TEST(test_small_negative_values);
  RUN_TEST(test_stability_time_and_reset);
  RUN_TEST(test_calibration_calculation);
  RUN_TEST(test_simulated_interrupt_sequence);
  UNITY_END();
}

#ifdef ARDUINO
void loop() { vTaskDelay(portMAX_DELAY); }
#else
int main(int, char**) {
  setup();
  return 0;
}
#endif
