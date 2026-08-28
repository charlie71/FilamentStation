#include <unity.h>

#include <cstddef>

#include "services/Sha256Hex.h"

using namespace filament_station;

void setUp() {}
void tearDown() {}

namespace {

constexpr char kValidHashLower[] =
    "c79e026ddbc0a720e67500cc3ede059680ee8a898cd5b7717f56dc7cb9e062e9";
// Deliberately wrong length (65 hex chars, not 64) to make the "too long"
// case unambiguous while still starting with 64 valid hex digits.
constexpr char kTooLongHash[] =
    "c79e026ddbc0a720e67500cc3ede059680ee8a898cd5b7717f56dc7cb9e062e91";

void testExtractsPlainLowercaseHash() {
  char out[65]{};
  TEST_ASSERT_TRUE(services::extractHexSha256(kValidHashLower, out));
  TEST_ASSERT_EQUAL_STRING(kValidHashLower, out);
}

void testExtractsAndLowercasesUppercaseHash() {
  char upper[65]{};
  for (std::size_t i = 0; i < 64; ++i) {
    const char c = kValidHashLower[i];
    upper[i] = (c >= 'a' && c <= 'f') ? static_cast<char>(c - 'a' + 'A') : c;
  }
  upper[64] = '\0';
  char out[65]{};
  TEST_ASSERT_TRUE(services::extractHexSha256(upper, out));
  TEST_ASSERT_EQUAL_STRING(kValidHashLower, out);
}

void testExtractsHashWithTrailingFilename() {
  // "sha256sum"-style output: "<hash>  <filename>".
  const char text[] =
      "c79e026ddbc0a720e67500cc3ede059680ee8a898cd5b7717f56dc7cb9e062e9  "
      "bambu_materials.json";
  char out[65]{};
  TEST_ASSERT_TRUE(services::extractHexSha256(text, out));
  TEST_ASSERT_EQUAL_STRING(kValidHashLower, out);
}

void testExtractsHashWithAsteriskAndFilename() {
  const char text[] =
      "c79e026ddbc0a720e67500cc3ede059680ee8a898cd5b7717f56dc7cb9e062e9 "
      "*bambu_materials.json";
  char out[65]{};
  TEST_ASSERT_TRUE(services::extractHexSha256(text, out));
  TEST_ASSERT_EQUAL_STRING(kValidHashLower, out);
}

void testRejectsTooShortHash() {
  const char shortHash[] = "c79e026ddbc0a720e67500cc3ede059680ee8a898cd5b77";
  char out[65]{};
  TEST_ASSERT_FALSE(services::extractHexSha256(shortHash, out));
}

void testAcceptsFirst64DigitsIgnoringExtraTrailingHexDigit() {
  // Not a "too long" rejection per se -- extractHexSha256() only ever reads
  // the first 64 characters, so a 65th hex digit right after the hash
  // (rather than a space-separated filename) is simply not part of the
  // extracted digest. Documents this intentional behavior.
  char out[65]{};
  TEST_ASSERT_TRUE(services::extractHexSha256(kTooLongHash, out));
  TEST_ASSERT_EQUAL_STRING(kValidHashLower, out);
}

void testRejectsNonHexCharacters() {
  const char text[] =
      "g79e026ddbc0a720e67500cc3ede059680ee8a898cd5b7717f56dc7cb9e062e9";
  char out[65]{};
  TEST_ASSERT_FALSE(services::extractHexSha256(text, out));
}

void testRejectsNull() {
  char out[65]{};
  TEST_ASSERT_FALSE(services::extractHexSha256(nullptr, out));
}

void testRejectsEmptyString() {
  char out[65]{};
  TEST_ASSERT_FALSE(services::extractHexSha256("", out));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testExtractsPlainLowercaseHash);
  RUN_TEST(testExtractsAndLowercasesUppercaseHash);
  RUN_TEST(testExtractsHashWithTrailingFilename);
  RUN_TEST(testExtractsHashWithAsteriskAndFilename);
  RUN_TEST(testRejectsTooShortHash);
  RUN_TEST(testAcceptsFirst64DigitsIgnoringExtraTrailingHexDigit);
  RUN_TEST(testRejectsNonHexCharacters);
  RUN_TEST(testRejectsNull);
  RUN_TEST(testRejectsEmptyString);
  return UNITY_END();
}
