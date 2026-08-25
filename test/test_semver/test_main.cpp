#include <unity.h>

#include "services/SemVer.h"

using filament_station::services::SemVer;
using filament_station::services::compareSemVer;
using filament_station::services::parseSemVer;

void setUp() {}
void tearDown() {}

void testParsesPlainVersion() {
  SemVer version{};
  TEST_ASSERT_TRUE(parseSemVer("1.2.3", version));
  TEST_ASSERT_EQUAL_UINT32(1, version.major);
  TEST_ASSERT_EQUAL_UINT32(2, version.minor);
  TEST_ASSERT_EQUAL_UINT32(3, version.patch);
  TEST_ASSERT_FALSE(version.hasSuffix);
}

void testParsesLeadingVAndSuffix() {
  SemVer version{};
  TEST_ASSERT_TRUE(parseSemVer("v0.1.0-dev", version));
  TEST_ASSERT_EQUAL_UINT32(0, version.major);
  TEST_ASSERT_EQUAL_UINT32(1, version.minor);
  TEST_ASSERT_EQUAL_UINT32(0, version.patch);
  TEST_ASSERT_TRUE(version.hasSuffix);
}

void testParsesBuildMetadataSuffix() {
  SemVer version{};
  TEST_ASSERT_TRUE(parseSemVer("2.0.0+build.5", version));
  TEST_ASSERT_TRUE(version.hasSuffix);
}

void testRejectsMalformedInput() {
  SemVer version{};
  TEST_ASSERT_FALSE(parseSemVer("", version));
  TEST_ASSERT_FALSE(parseSemVer("1.2", version));
  TEST_ASSERT_FALSE(parseSemVer("1..3", version));
  TEST_ASSERT_FALSE(parseSemVer("a.b.c", version));
  TEST_ASSERT_FALSE(parseSemVer(nullptr, version));
}

void testCompareCoreVersions() {
  SemVer older{};
  SemVer newer{};
  TEST_ASSERT_TRUE(parseSemVer("1.2.3", older));
  TEST_ASSERT_TRUE(parseSemVer("1.3.0", newer));
  TEST_ASSERT_TRUE(compareSemVer(older, newer) < 0);
  TEST_ASSERT_TRUE(compareSemVer(newer, older) > 0);
  TEST_ASSERT_EQUAL_INT(0, compareSemVer(older, older));
}

void testSuffixCountsAsOlderThanSameCoreVersion() {
  SemVer withSuffix{};
  SemVer release{};
  TEST_ASSERT_TRUE(parseSemVer("1.0.0-rc1", withSuffix));
  TEST_ASSERT_TRUE(parseSemVer("1.0.0", release));
  TEST_ASSERT_TRUE(compareSemVer(withSuffix, release) < 0);
  TEST_ASSERT_TRUE(compareSemVer(release, withSuffix) > 0);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testParsesPlainVersion);
  RUN_TEST(testParsesLeadingVAndSuffix);
  RUN_TEST(testParsesBuildMetadataSuffix);
  RUN_TEST(testRejectsMalformedInput);
  RUN_TEST(testCompareCoreVersions);
  RUN_TEST(testSuffixCountsAsOlderThanSameCoreVersion);
  return UNITY_END();
}
