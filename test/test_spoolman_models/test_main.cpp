#include <unity.h>

#include <type_traits>
#include <cstring>

#include "models/SpoolmanSpool.h"

void setUp() {}
void tearDown() {}

void testCompactSpoolModelIsQueueSafeValueType() {
  using filament_station::models::SpoolmanSpool;
  TEST_ASSERT_TRUE(std::is_trivially_copyable<SpoolmanSpool>::value);
  SpoolmanSpool spool{};
  spool.id = 42;
  spool.remainingWeightGrams = 642.0F;
  spool.colorCount = 2;
  std::strcpy(spool.colorHex[0], "00A651");
  std::strcpy(spool.colorHex[1], "FFFFFF");
  TEST_ASSERT_EQUAL_UINT32(42, spool.id);
  TEST_ASSERT_EQUAL_FLOAT(642.0F, spool.remainingWeightGrams);
  TEST_ASSERT_EQUAL_UINT8(2, spool.colorCount);
  TEST_ASSERT_EQUAL_STRING("00A651", spool.colorHex[0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testCompactSpoolModelIsQueueSafeValueType);
  return UNITY_END();
}
