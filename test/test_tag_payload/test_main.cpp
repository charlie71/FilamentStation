#include <unity.h>

#include <cstdint>
#include <cstring>

#include "nfc/TagParserRegistry.h"
#include "services/NfcPayload.h"

using filament_station::models::RawTagData;
using filament_station::models::TagDefinition;
using filament_station::models::TagFormat;
using filament_station::nfc::ITagParser;
using filament_station::nfc::TagParseResult;
using filament_station::nfc::TagParserRegistry;
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

RawTagData rawNdef(const std::uint8_t* bytes, std::size_t size,
                   bool writable = true) {
  RawTagData tag{};
  tag.technology = filament_station::models::TagTechnology::OtherIso14443A;
  tag.uid[0] = 0x04;
  tag.uid[1] = 0xA2;
  tag.uidLength = 2;
  tag.ndefPresent = true;
  tag.ndefReadable = true;
  tag.hardwareWritable = writable;
  tag.ndefLength = static_cast<std::uint16_t>(size);
  std::memcpy(tag.ndef, bytes, size);
  return tag;
}

RawTagData textNdef(const char* text, bool writable = true) {
  std::uint8_t bytes[96]{};
  const std::size_t textLength = std::strlen(text);
  const std::size_t payloadLength = 3 + textLength;
  const std::size_t recordLength = 4 + payloadLength;
  std::size_t p = 0;
  bytes[p++] = 0x03;
  bytes[p++] = static_cast<std::uint8_t>(recordLength);
  bytes[p++] = 0xD1;
  bytes[p++] = 0x01;
  bytes[p++] = static_cast<std::uint8_t>(payloadLength);
  bytes[p++] = 'T';
  bytes[p++] = 0x02;
  bytes[p++] = 'e';
  bytes[p++] = 'n';
  std::memcpy(bytes + p, text, textLength);
  p += textLength;
  bytes[p++] = 0xFE;
  return rawNdef(bytes, p, writable);
}

void test_valid_spoolman_42_is_filament_station() {
  const auto result = TagParserRegistry{}.parse(textNdef("spoolman:42"));
  TEST_ASSERT_EQUAL(TagFormat::FilamentStation, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_TRUE(result.definition.hasSpoolId);
  TEST_ASSERT_EQUAL_UINT32(42, result.definition.spoolId);
  TEST_ASSERT_TRUE(result.writable);
}

void test_empty_spoolman_id_is_not_accepted() {
  const auto result = TagParserRegistry{}.parse(textNdef("spoolman:"));
  TEST_ASSERT_EQUAL(TagFormat::Unknown, result.format);
  TEST_ASSERT_FALSE(result.knownFormat);
  TEST_ASSERT_FALSE(result.definition.hasSpoolId);
  TEST_ASSERT_FALSE(result.writable);
}

void test_invalid_spoolman_id_is_not_accepted() {
  const auto alpha = TagParserRegistry{}.parse(textNdef("spoolman:abc"));
  const auto overflow =
      TagParserRegistry{}.parse(textNdef("spoolman:4294967296"));
  TEST_ASSERT_EQUAL(TagFormat::Unknown, alpha.format);
  TEST_ASSERT_EQUAL(TagFormat::Unknown, overflow.format);
  TEST_ASSERT_FALSE(alpha.definition.hasSpoolId);
  TEST_ASSERT_FALSE(overflow.definition.hasSpoolId);
}

void test_empty_ndef_is_recognized() {
  const std::uint8_t empty[] = {0x03, 0x00, 0xFE};
  const auto result = TagParserRegistry{}.parse(rawNdef(empty, sizeof(empty)));
  TEST_ASSERT_EQUAL(TagFormat::EmptyNdef, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_TRUE(result.writable);
  TEST_ASSERT_TRUE(result.erasable);
}

void test_unknown_tag_has_no_definition_or_write_capability() {
  const auto result = TagParserRegistry{}.parse(textNdef("other:data"));
  TEST_ASSERT_EQUAL(TagFormat::Unknown, result.format);
  TEST_ASSERT_FALSE(result.knownFormat);
  TEST_ASSERT_FALSE(result.definition.hasSpoolId);
  TEST_ASSERT_FALSE(result.writable);
  TEST_ASSERT_FALSE(result.erasable);
}

class MatchingParser final : public ITagParser {
 public:
  explicit MatchingParser(TagFormat format) : format_(format) {}
  TagFormat format() const override { return format_; }
  bool canParse(const RawTagData&) const override { return true; }
  TagParseResult parse(const RawTagData&, TagDefinition& result) const override {
    result.format = format_;
    return TagParseResult::Parsed;
  }

 private:
  TagFormat format_;
};

void test_parser_order_uses_first_successful_parser() {
  MatchingParser first{TagFormat::Legacy};
  MatchingParser second{TagFormat::BambuLab};
  const ITagParser* parsers[] = {&first, &second};
  const TagParserRegistry registry{parsers, 2};
  const auto result = registry.parse(textNdef("irrelevant"));
  TEST_ASSERT_EQUAL(TagFormat::Legacy, result.format);
}

class RejectingParser final : public ITagParser {
 public:
  TagFormat format() const override { return TagFormat::Legacy; }
  bool canParse(const RawTagData&) const override { return false; }
  TagParseResult parse(const RawTagData&, TagDefinition&) const override {
    return TagParseResult::NoMatch;
  }
};

void test_unknown_data_does_not_match_rejecting_parser() {
  RejectingParser rejecting;
  const ITagParser* parsers[] = {&rejecting};
  const auto result = TagParserRegistry{parsers, 1}.parse(textNdef("unknown"));
  TEST_ASSERT_EQUAL(TagFormat::Unknown, result.format);
  TEST_ASSERT_FALSE(result.knownFormat);
}

void test_bambu_marker_is_always_read_only() {
  const auto result =
      TagParserRegistry{}.parse(textNdef("Bambu Lab", true));
  TEST_ASSERT_EQUAL(TagFormat::BambuLab, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_FALSE(result.writable);
  TEST_ASSERT_FALSE(result.erasable);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_spoolman_payload_round_trip);
  RUN_TEST(test_legacy_payload_is_recognized);
  RUN_TEST(test_bambu_text_is_recognized);
  RUN_TEST(test_malformed_payload_is_rejected);
  RUN_TEST(test_valid_spoolman_42_is_filament_station);
  RUN_TEST(test_empty_spoolman_id_is_not_accepted);
  RUN_TEST(test_invalid_spoolman_id_is_not_accepted);
  RUN_TEST(test_empty_ndef_is_recognized);
  RUN_TEST(test_unknown_tag_has_no_definition_or_write_capability);
  RUN_TEST(test_parser_order_uses_first_successful_parser);
  RUN_TEST(test_unknown_data_does_not_match_rejecting_parser);
  RUN_TEST(test_bambu_marker_is_always_read_only);
  return UNITY_END();
}
