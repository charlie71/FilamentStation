/**
 * @file
 * @brief Implements the five built-in ITagParser classes declared in
 *        nfc/TagParsers.h.
 */
#include "nfc/TagParsers.h"

#include <cstdio>
#include <cstring>

#include "services/NfcPayload.h"
#include "nfc/OpenPrintTag.h"
#include "nfc/OpenTag3D.h"

namespace filament_station {
namespace nfc {
namespace {

/// @brief Parses the tag's Type 2 NDEF content, if any.
/// @param tag Raw tag data as read from the PN532.
/// @return Parsed payload info, or a default-constructed (Empty/Invalid) result if no NDEF is present.
services::NfcPayloadInfo payload(const models::RawTagData& tag) {
  if (!tag.ndefPresent || !tag.ndefReadable) return {};
  return services::parseType2Ndef(tag.ndef, tag.ndefLength);
}

/// @brief Shared parse() body for the simple NDEF-marker-based formats
///        (FilamentStation, Bambu marker, legacy).
/// @param tag Raw tag data as read from the PN532.
/// @param expected Payload type this parser recognizes.
/// @param format Format identifier to stamp onto `result` on success.
/// @param description Human-readable source description to record.
/// @param result Out parameter receiving the decoded fields.
/// @return TagParseResult::Parsed if `tag`'s payload matched `expected`, otherwise TagParseResult::NoMatch.
TagParseResult parseKnownPayload(const models::RawTagData& tag,
                                 services::NfcPayloadType expected,
                                 models::TagFormat format,
                                 const char* description,
                                 models::TagDefinition& result) {
  const auto info = payload(tag);
  if (info.type != expected) return TagParseResult::NoMatch;
  result = {};
  result.format = format;
  result.hasSpoolId = info.spoolId != 0;
  result.spoolId = info.spoolId;
  std::snprintf(result.sourceDescription, sizeof(result.sourceDescription),
                "%s", description);
  return TagParseResult::Parsed;
}

}  // namespace

models::TagFormat FilamentStationTagParser::format() const {
  return models::TagFormat::FilamentStation;
}
bool FilamentStationTagParser::canParse(const models::RawTagData& tag) const {
  return payload(tag).type == services::NfcPayloadType::Spoolman;
}
TagParseResult FilamentStationTagParser::parse(
    const models::RawTagData& tag, models::TagDefinition& result) const {
  return parseKnownPayload(tag, services::NfcPayloadType::Spoolman, format(),
                           "FilamentStation spoolman NDEF", result);
}

models::TagFormat BambuLabTagParser::format() const {
  return models::TagFormat::BambuLab;
}
bool BambuLabTagParser::canParse(const models::RawTagData& tag) const {
  constexpr std::uint32_t kRequiredBlocks =
      (1UL << 2) | (1UL << 4) | (1UL << 5) | (1UL << 6);
  return payload(tag).type == services::NfcPayloadType::Bambu ||
         (tag.technology == models::TagTechnology::MifareClassic1K &&
          (tag.mifareBlockMask & kRequiredBlocks) == kRequiredBlocks);
}
TagParseResult BambuLabTagParser::parse(
    const models::RawTagData& tag, models::TagDefinition& result) const {
  if (tag.technology == models::TagTechnology::MifareClassic1K &&
      (tag.mifareBlockMask & ((1UL << 2) | (1UL << 4) | (1UL << 5) |
                              (1UL << 6))) ==
          ((1UL << 2) | (1UL << 4) | (1UL << 5) | (1UL << 6))) {
    result = {};
    result.format = format();
    std::snprintf(result.vendor, sizeof(result.vendor), "Bambu Lab");
    auto copyText = [](char* destination, std::size_t capacity,
                       const std::uint8_t* source, std::size_t length) {
      const std::size_t count = length < capacity - 1 ? length : capacity - 1;
      std::memcpy(destination, source, count);
      destination[count] = '\0';
      while (count > 0 &&
             (destination[std::strlen(destination) - 1] == ' ' ||
              destination[std::strlen(destination) - 1] == '\0')) {
        const std::size_t used = std::strlen(destination);
        if (used == 0) break;
        destination[used - 1] = '\0';
      }
    };
    copyText(result.material, sizeof(result.material), tag.mifareBlocks[2], 16);
    copyText(result.filamentName, sizeof(result.filamentName),
             tag.mifareBlocks[4], 16);
    const auto* color = tag.mifareBlocks[5];
    std::snprintf(result.colorCode, sizeof(result.colorCode), "#%02X%02X%02X",
                  color[0], color[1], color[2]);
    const std::uint16_t nominalWeight =
        color[4] | (static_cast<std::uint16_t>(color[5]) << 8);
    if (nominalWeight <= 5000U)
      result.nominalFilamentWeightG = static_cast<float>(nominalWeight);
    const auto* temperatures = tag.mifareBlocks[6];
    const std::uint16_t maximumTemperature = static_cast<std::uint16_t>(
        temperatures[8] | (static_cast<std::uint16_t>(temperatures[9]) << 8));
    const std::uint16_t minimumTemperature = static_cast<std::uint16_t>(
        temperatures[10] | (static_cast<std::uint16_t>(temperatures[11]) << 8));
    if (minimumTemperature <= maximumTemperature && maximumTemperature <= 500U) {
      result.nozzleTempMaxC = static_cast<std::int16_t>(maximumTemperature);
      result.nozzleTempMinC = static_cast<std::int16_t>(minimumTemperature);
    }
    std::snprintf(result.sourceDescription, sizeof(result.sourceDescription),
                  "Bambu MIFARE Classic 1K public block format");
    return TagParseResult::Parsed;
  }
  return parseKnownPayload(tag, services::NfcPayloadType::Bambu, format(),
                           "Bambu marker in NDEF text", result);
}

models::TagFormat OpenPrintTagParser::format() const {
  return models::TagFormat::OpenPrintTag;
}
bool OpenPrintTagParser::canParse(const models::RawTagData& tag) const {
  return tag.ndefPresent && tag.ndefReadable &&
         containsOpenPrintTagMimeRecord(tag.ndef, tag.ndefLength);
}
TagParseResult OpenPrintTagParser::parse(
    const models::RawTagData& tag, models::TagDefinition& result) const {
  if (!canParse(tag)) return TagParseResult::NoMatch;
  return parseOpenPrintTagNdef(tag.ndef, tag.ndefLength, result)
             ? TagParseResult::Parsed
             : TagParseResult::Invalid;
}

models::TagFormat OpenTag3DParser::format() const {
  return models::TagFormat::OpenTag3D;
}
bool OpenTag3DParser::canParse(const models::RawTagData& tag) const {
  return tag.ndefPresent && tag.ndefReadable &&
         containsOpenTag3DMimeRecord(tag.ndef, tag.ndefLength);
}
TagParseResult OpenTag3DParser::parse(const models::RawTagData& tag,
                                      models::TagDefinition& result) const {
  if (!canParse(tag)) return TagParseResult::NoMatch;
  return parseOpenTag3DNdef(tag.ndef, tag.ndefLength, result)
             ? TagParseResult::Parsed
             : TagParseResult::Invalid;
}

models::TagFormat LegacyTagParser::format() const {
  return models::TagFormat::Legacy;
}
bool LegacyTagParser::canParse(const models::RawTagData& tag) const {
  return payload(tag).type == services::NfcPayloadType::Legacy;
}
TagParseResult LegacyTagParser::parse(const models::RawTagData& tag,
                                      models::TagDefinition& result) const {
  const auto parsed =
      parseKnownPayload(tag, services::NfcPayloadType::Legacy, format(),
                        "Legacy spool NDEF", result);
  if (parsed == TagParseResult::Parsed) {
    // This parser recognizes the documented plain-text spool:<id> payload.
    // It is the only legacy representation currently verified as safe to
    // replace with spoolman:<id> on a writable native NTAG.
    result.safeToRewriteAsFilamentStation = true;
  }
  return parsed;
}

}  // namespace nfc
}  // namespace filament_station
