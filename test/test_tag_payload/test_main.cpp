#include <unity.h>

#include <cstdint>

#include "services/NfcPayload.h"

using filament_station::services::NfcPayloadType;

void setUp() {}
void tearDown() {}

void test_spoolman_payload_round_trip() {
  std::uint8_t bytes[64]{};
  std::size_t size = 0;
  TEST_ASSERT_TRUE(filament_station::services::buildSpoolmanType2Ndef(
      4294967295U, bytes, sizeof(bytes), size));
  TEST_ASSERT_EQUAL_UINT(0, size % 4);
  const auto result =
      filament_station::services::parseType2Ndef(bytes, size);
  TEST_ASSERT_EQUAL(NfcPayloadType::Spoolman, result.type);
  TEST_ASSERT_EQUAL_UINT32(4294967295U, result.spoolId);
}

void test_legacy_payload_is_recognized() {
  const std::uint8_t bytes[] = {0x03, 0x0F, 0xD1, 0x01, 0x0B, 'T', 0x02,
                                'e',  'n',  's',  'p',  'o', 'o', 'l', ':',
                                '4',  '2',  0xFE};
  const auto result =
      filament_station::services::parseType2Ndef(bytes, sizeof(bytes));
  TEST_ASSERT_EQUAL(NfcPayloadType::Legacy, result.type);
  TEST_ASSERT_EQUAL_UINT32(42, result.spoolId);
}

void test_bambu_text_is_recognized() {
  const std::uint8_t bytes[] = {0x03, 0x10, 0xD1, 0x01, 0x0C, 'T', 0x02,
                                'e',  'n',  'B',  'a',  'm', 'b', 'u', ' ',
                                'L',  'a',  'b',  0xFE};
  const auto result =
      filament_station::services::parseType2Ndef(bytes, sizeof(bytes));
  TEST_ASSERT_EQUAL(NfcPayloadType::Bambu, result.type);
}

void test_malformed_payload_is_rejected() {
  const std::uint8_t bytes[] = {0x03, 0x20, 0xD1};
  TEST_ASSERT_EQUAL(NfcPayloadType::Invalid,
                    filament_station::services::parseType2Ndef(bytes,
                                                                sizeof(bytes))
                        .type);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_spoolman_payload_round_trip);
  RUN_TEST(test_legacy_payload_is_recognized);
  RUN_TEST(test_bambu_text_is_recognized);
  RUN_TEST(test_malformed_payload_is_rejected);
  return UNITY_END();
}
