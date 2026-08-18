#include <unity.h>
#include <cstdio>

#include "services/SpoolmanUrl.h"

using filament_station::services::SpoolmanUrlParts;
using filament_station::services::buildNormalizedSpoolmanUrl;
using filament_station::services::parseNormalizedSpoolmanUrl;

void setUp() {}
void tearDown() {}

void testBuildNormalizesTrailingSlash() {
  SpoolmanUrlParts parts{};
  std::snprintf(parts.protocol, sizeof(parts.protocol), "http");
  std::snprintf(parts.host, sizeof(parts.host), "spoolman.local");
  std::snprintf(parts.port, sizeof(parts.port), "7912");
  std::snprintf(parts.basePath, sizeof(parts.basePath), "/api/v1/");
  char url[128]{};
  TEST_ASSERT_TRUE(buildNormalizedSpoolmanUrl(parts, url, sizeof(url)));
  TEST_ASSERT_EQUAL_STRING("http://spoolman.local:7912/api/v1", url);
}

void testParseNormalizedUrl() {
  SpoolmanUrlParts parts{};
  TEST_ASSERT_TRUE(parseNormalizedSpoolmanUrl(
      "https://server.example:443/api/v1", parts));
  TEST_ASSERT_EQUAL_STRING("https", parts.protocol);
  TEST_ASSERT_EQUAL_STRING("server.example", parts.host);
  TEST_ASSERT_EQUAL_STRING("443", parts.port);
  TEST_ASSERT_EQUAL_STRING("/api/v1", parts.basePath);
}

void testRejectsInvalidPort() {
  SpoolmanUrlParts parts{};
  std::snprintf(parts.protocol, sizeof(parts.protocol), "http");
  std::snprintf(parts.host, sizeof(parts.host), "spoolman.local");
  std::snprintf(parts.port, sizeof(parts.port), "70000");
  std::snprintf(parts.basePath, sizeof(parts.basePath), "/api/v1");
  char url[128]{};
  TEST_ASSERT_FALSE(buildNormalizedSpoolmanUrl(parts, url, sizeof(url)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testBuildNormalizesTrailingSlash);
  RUN_TEST(testParseNormalizedUrl);
  RUN_TEST(testRejectsInvalidPort);
  return UNITY_END();
}
