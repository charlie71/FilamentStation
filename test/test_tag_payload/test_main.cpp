#include <unity.h>

#include <cstdint>
#include <cstring>

#include "nfc/TagParserRegistry.h"
#include "nfc/TagWritePolicy.h"
#include "openprinttag_official_vector.h"
#include "services/NfcPayload.h"
#include "services/Ntag21x.h"

using filament_station::models::RawTagData;
using filament_station::models::TagDefinition;
using filament_station::models::TagFormat;
using filament_station::models::TagTechnology;
using filament_station::nfc::ITagParser;
using filament_station::nfc::TagParseResult;
using filament_station::nfc::TagParserRegistry;
using filament_station::services::NfcPayloadType;

void setUp() {}
void tearDown() {}

RawTagData makeOpenTag3DVector() {
  RawTagData raw{};
  raw.technology = TagTechnology::Ntag215;
  raw.ndefPresent = true;
  raw.ndefReadable = true;
  raw.hardwareWritable = true;
  constexpr char mime[] = "application/opentag3d";
  constexpr std::size_t payloadSize = 0xBB;
  constexpr std::size_t recordSize = 3 + sizeof(mime) - 1 + payloadSize;
  std::size_t position = 0;
  raw.ndef[position++] = 0xE1;
  raw.ndef[position++] = 0x10;
  raw.ndef[position++] = 0x3E;
  raw.ndef[position++] = 0x00;
  raw.ndef[position++] = 0x03;
  raw.ndef[position++] = static_cast<std::uint8_t>(recordSize);
  raw.ndef[position++] = 0xD2;  // MB, ME, SR, MIME
  raw.ndef[position++] = static_cast<std::uint8_t>(sizeof(mime) - 1);
  raw.ndef[position++] = static_cast<std::uint8_t>(payloadSize);
  std::memcpy(raw.ndef + position, mime, sizeof(mime) - 1);
  position += sizeof(mime) - 1;
  std::uint8_t* payload = raw.ndef + position;
  payload[0x00] = 0x03;  // Version 1.001, big endian
  payload[0x01] = 0xE9;
  std::memcpy(payload + 0x02, "PETG", 4);
  std::memcpy(payload + 0x07, "HF", 2);
  std::memcpy(payload + 0x1B, "Polar Filament", 14);
  std::memcpy(payload + 0x2B, "Electric Blue", 13);
  payload[0x4B] = 0x12;
  payload[0x4C] = 0x57;
  payload[0x4D] = 0xC4;
  payload[0x4E] = 0xFF;
  payload[0x5C] = 0x06;  // 1.750 mm
  payload[0x5D] = 0xD6;
  payload[0x5E] = 0x03;  // 1000 g
  payload[0x5F] = 0xE8;
  payload[0x60] = 48;    // 240 C
  payload[0x61] = 16;    // 80 C
  payload[0x62] = 0x04;  // 1.270 g/cm3
  payload[0x63] = 0xF6;
  payload[0xAC] = 0x00;  // 245 g empty spool
  payload[0xAD] = 0xF5;
  payload[0xAE] = 0x03;  // optional measured weight: 1002 g
  payload[0xAF] = 0xEA;
  payload[0xB4] = 46;    // 230 C minimum
  payload[0xB5] = 50;    // 250 C maximum
  payload[0xA6] = 0x7B;  // unrelated optional/reserved data is ignored
  position += payloadSize;
  raw.ndef[position++] = 0xFE;
  raw.ndefLength = static_cast<std::uint16_t>(position);
  return raw;
}

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

void test_original_bambu_mifare_definition_is_normalized_and_read_only() {
  RawTagData raw{};
  raw.technology = TagTechnology::MifareClassic1K;
  raw.uidLength = 4;
  raw.uid[0] = 0x12;
  raw.uid[1] = 0x34;
  raw.uid[2] = 0x56;
  raw.uid[3] = 0x78;
  raw.hardwareWritable = true;
  raw.mifareBlockMask =
      (1UL << 2) | (1UL << 4) | (1UL << 5) | (1UL << 6);
  std::memcpy(raw.mifareBlocks[2], "PLA", 3);
  std::memcpy(raw.mifareBlocks[4], "PLA Basic", 9);
  raw.mifareBlocks[5][0] = 0xF5;
  raw.mifareBlocks[5][1] = 0xF5;
  raw.mifareBlocks[5][2] = 0xF5;
  raw.mifareBlocks[5][3] = 0xFF;
  raw.mifareBlocks[5][4] = 0xE8;
  raw.mifareBlocks[5][5] = 0x03;
  raw.mifareBlocks[6][8] = 230;
  raw.mifareBlocks[6][10] = 190;

  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_EQUAL(TagFormat::BambuLab, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_FALSE(result.writable);
  TEST_ASSERT_FALSE(result.erasable);
  TEST_ASSERT_FALSE(filament_station::nfc::mayWriteTag(result));
  TEST_ASSERT_FALSE(filament_station::nfc::mayEraseTag(result));
  TEST_ASSERT_EQUAL_STRING("Bambu Lab", result.definition.vendor);
  TEST_ASSERT_EQUAL_STRING("PLA", result.definition.material);
  TEST_ASSERT_EQUAL_STRING("PLA Basic", result.definition.filamentName);
  TEST_ASSERT_EQUAL_STRING("", result.definition.colorName);
  TEST_ASSERT_EQUAL_STRING("#F5F5F5", result.definition.colorCode);
  TEST_ASSERT_EQUAL_FLOAT(1000.0F,
                          result.definition.nominalFilamentWeightG);
  TEST_ASSERT_EQUAL_INT16(190, result.definition.nozzleTempMinC);
  TEST_ASSERT_EQUAL_INT16(230, result.definition.nozzleTempMaxC);
}

void test_official_openprinttag_vector_is_normalized_and_read_only() {
  RawTagData raw{};
  raw.technology = TagTechnology::OtherIso14443A;
  raw.ndefPresent = true;
  raw.ndefReadable = true;
  raw.hardwareWritable = true;
  raw.ndefLength = static_cast<std::uint16_t>(kOpenPrintTagOfficialVectorSize);
  std::memcpy(raw.ndef, kOpenPrintTagOfficialVector,
              kOpenPrintTagOfficialVectorSize);
  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_EQUAL(TagFormat::OpenPrintTag, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_TRUE(result.payloadValid);
  TEST_ASSERT_FALSE(result.writable);
  TEST_ASSERT_FALSE(result.erasable);
  TEST_ASSERT_FALSE(filament_station::nfc::mayWriteTag(result));
  TEST_ASSERT_FALSE(filament_station::nfc::mayEraseTag(result));
  TEST_ASSERT_EQUAL_STRING("Prusament", result.definition.vendor);
  TEST_ASSERT_EQUAL_STRING("PLA Prusa Galaxy Black",
                           result.definition.filamentName);
  TEST_ASSERT_EQUAL_STRING("PLA", result.definition.material);
  TEST_ASSERT_EQUAL_STRING("#3D3E3D", result.definition.colorCode);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 1012.0F,
                           result.definition.nominalFilamentWeightG);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 280.0F,
                           result.definition.emptySpoolWeightG);
  TEST_ASSERT_EQUAL_INT16(205, result.definition.nozzleTempMinC);
  TEST_ASSERT_EQUAL_INT16(225, result.definition.nozzleTempMaxC);
}

void test_openprinttag_unknown_optional_field_is_ignored() {
  std::uint8_t vector[sizeof(kOpenPrintTagOfficialVector)]{};
  std::memcpy(vector, kOpenPrintTagOfficialVector, sizeof(vector));
  // Im offiziellen Vektor ist das letzte Element der indefinite Main-Map 0xFF.
  // Davor wird ein unbekanntes Feld 1000 mit einem verschachtelten Array
  // eingefuegt; die nachfolgenden Paddingbytes bleiben unveraendert.
  const std::size_t mainBreak = 245;
  vector[mainBreak + 0] = 0x19;
  vector[mainBreak + 1] = 0x03;
  vector[mainBreak + 2] = 0xE8;
  vector[mainBreak + 3] = 0x82;
  vector[mainBreak + 4] = 0x01;
  vector[mainBreak + 5] = 0xA1;
  vector[mainBreak + 6] = 0x01;
  vector[mainBreak + 7] = 0x02;
  vector[mainBreak + 8] = 0xFF;
  RawTagData raw{};
  raw.technology = TagTechnology::OtherIso14443A;
  raw.ndefPresent = true;
  raw.ndefReadable = true;
  raw.ndefLength = sizeof(vector);
  std::memcpy(raw.ndef, vector, sizeof(vector));
  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_EQUAL(TagFormat::OpenPrintTag, result.format);
  TEST_ASSERT_TRUE(result.payloadValid);
  TEST_ASSERT_EQUAL_STRING("Prusament", result.definition.vendor);
}

void test_foreign_mime_is_not_openprinttag() {
  RawTagData raw{};
  raw.technology = TagTechnology::OtherIso14443A;
  raw.ndefPresent = true;
  raw.ndefReadable = true;
  raw.ndefLength = static_cast<std::uint16_t>(kOpenPrintTagOfficialVectorSize);
  std::memcpy(raw.ndef, kOpenPrintTagOfficialVector,
              kOpenPrintTagOfficialVectorSize);
  raw.ndef[40] = 'x';
  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_NOT_EQUAL(TagFormat::OpenPrintTag, result.format);
}

void test_documented_opentag3d_vector_is_normalized_and_read_only() {
  const auto raw = makeOpenTag3DVector();
  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_EQUAL(TagFormat::OpenTag3D, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_TRUE(result.payloadValid);
  TEST_ASSERT_FALSE(result.writable);
  TEST_ASSERT_FALSE(result.erasable);
  TEST_ASSERT_FALSE(filament_station::nfc::mayWriteTag(result));
  TEST_ASSERT_EQUAL_STRING("Polar Filament", result.definition.vendor);
  TEST_ASSERT_EQUAL_STRING("PETG", result.definition.material);
  TEST_ASSERT_EQUAL_STRING("PETG HF", result.definition.filamentName);
  TEST_ASSERT_EQUAL_STRING("Electric Blue", result.definition.colorName);
  TEST_ASSERT_EQUAL_STRING("#1257C4", result.definition.colorCode);
  TEST_ASSERT_EQUAL_FLOAT(1000.0F,
                          result.definition.nominalFilamentWeightG);
  TEST_ASSERT_EQUAL_FLOAT(245.0F, result.definition.emptySpoolWeightG);
  TEST_ASSERT_EQUAL_INT16(230, result.definition.nozzleTempMinC);
  TEST_ASSERT_EQUAL_INT16(250, result.definition.nozzleTempMaxC);
}

void test_opentag3d_new_major_version_is_rejected_without_crash() {
  auto raw = makeOpenTag3DVector();
  constexpr std::size_t payloadOffset = 4 + 2 + 3 + 21;
  raw.ndef[payloadOffset] = 0x07;
  raw.ndef[payloadOffset + 1] = 0xD0;  // 2.000
  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_EQUAL(TagFormat::OpenTag3D, result.format);
  TEST_ASSERT_TRUE(result.knownFormat);
  TEST_ASSERT_FALSE(result.payloadValid);
  TEST_ASSERT_FALSE(result.writable);
}

void test_foreign_mime_is_not_opentag3d() {
  auto raw = makeOpenTag3DVector();
  raw.ndef[9] = 'x';
  const auto result = TagParserRegistry{}.parse(raw);
  TEST_ASSERT_NOT_EQUAL(TagFormat::OpenTag3D, result.format);
}

void test_ntag21x_versions_and_capacities_are_identified() {
  std::uint8_t version[] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x0F, 0x03};
  std::uint8_t capability[] = {0xE1, 0x10, 0x12, 0x00};
  TEST_ASSERT_EQUAL(filament_station::models::TagTechnology::Ntag213,
                    filament_station::services::identifyNtag21x(
                        version, sizeof(version), capability));
  version[6] = 0x11;
  capability[2] = 0x3E;
  TEST_ASSERT_EQUAL(filament_station::models::TagTechnology::Ntag215,
                    filament_station::services::identifyNtag21x(
                        version, sizeof(version), capability));
  version[6] = 0x13;
  capability[2] = 0x6D;
  TEST_ASSERT_EQUAL(filament_station::models::TagTechnology::Ntag216,
                    filament_station::services::identifyNtag21x(
                        version, sizeof(version), capability));
}

void test_ntag_mismatched_or_locked_metadata_is_not_writable() {
  const std::uint8_t capability[] = {0xE1, 0x10, 0x3E, 0x00};
  const std::uint8_t unlocked[] = {0x00, 0x00, 0x00};
  TEST_ASSERT_TRUE(filament_station::services::ntag21xRangeWritable(
      filament_station::models::TagTechnology::Ntag215, 14, capability,
      unlocked, unlocked, 0xFF));
  const std::uint8_t locked[] = {0x01, 0x00, 0x00};
  TEST_ASSERT_FALSE(filament_station::services::ntag21xRangeWritable(
      filament_station::models::TagTechnology::Ntag215, 14, capability,
      locked, unlocked, 0xFF));
  TEST_ASSERT_FALSE(filament_station::services::ntag21xRangeWritable(
      filament_station::models::TagTechnology::Ntag215, 14, capability,
      unlocked, unlocked, 4));
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
  RUN_TEST(test_original_bambu_mifare_definition_is_normalized_and_read_only);
  RUN_TEST(test_official_openprinttag_vector_is_normalized_and_read_only);
  RUN_TEST(test_openprinttag_unknown_optional_field_is_ignored);
  RUN_TEST(test_foreign_mime_is_not_openprinttag);
  RUN_TEST(test_documented_opentag3d_vector_is_normalized_and_read_only);
  RUN_TEST(test_opentag3d_new_major_version_is_rejected_without_crash);
  RUN_TEST(test_foreign_mime_is_not_opentag3d);
  RUN_TEST(test_ntag21x_versions_and_capacities_are_identified);
  RUN_TEST(test_ntag_mismatched_or_locked_metadata_is_not_writable);
  return UNITY_END();
}
