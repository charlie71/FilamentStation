#include <Arduino.h>
#include <unity.h>

#include "services/ScaleFilter.h"

namespace {

using filament_station::services::ScaleFilter;
using filament_station::services::ScaleFilterConfig;

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
  TEST_ASSERT_EQUAL_INT32(1000, filter.process(1005, 20).value);
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

}  // namespace

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_moving_average);
  RUN_TEST(test_low_pass);
  RUN_TEST(test_outlier_and_confirmed_step);
  RUN_TEST(test_small_negative_values);
  RUN_TEST(test_stability_time_and_reset);
  UNITY_END();
}

void loop() { vTaskDelay(portMAX_DELAY); }
