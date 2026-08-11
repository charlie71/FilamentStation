#include "nfc/TagParsers.h"

#include <cstdio>

#include "services/NfcPayload.h"

namespace filament_station {
namespace nfc {
namespace {

services::NfcPayloadInfo payload(const models::RawTagData& tag) {
  if (!tag.ndefPresent || !tag.ndefReadable) return {};
  return services::parseType2Ndef(tag.ndef, tag.ndefLength);
}

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
  return payload(tag).type == services::NfcPayloadType::Bambu;
}
TagParseResult BambuLabTagParser::parse(
    const models::RawTagData& tag, models::TagDefinition& result) const {
  return parseKnownPayload(tag, services::NfcPayloadType::Bambu, format(),
                           "Bambu marker in NDEF text", result);
}

models::TagFormat OpenPrintTagParser::format() const {
  return models::TagFormat::OpenPrintTag;
}
bool OpenPrintTagParser::canParse(const models::RawTagData&) const {
  return false;
}
TagParseResult OpenPrintTagParser::parse(
    const models::RawTagData&, models::TagDefinition&) const {
  return TagParseResult::NoMatch;
}

models::TagFormat OpenTag3DParser::format() const {
  return models::TagFormat::OpenTag3D;
}
bool OpenTag3DParser::canParse(const models::RawTagData&) const {
  return false;
}
TagParseResult OpenTag3DParser::parse(const models::RawTagData&,
                                      models::TagDefinition&) const {
  return TagParseResult::NoMatch;
}

models::TagFormat LegacyTagParser::format() const {
  return models::TagFormat::Legacy;
}
bool LegacyTagParser::canParse(const models::RawTagData& tag) const {
  return payload(tag).type == services::NfcPayloadType::Legacy;
}
TagParseResult LegacyTagParser::parse(const models::RawTagData& tag,
                                      models::TagDefinition& result) const {
  return parseKnownPayload(tag, services::NfcPayloadType::Legacy, format(),
                           "Legacy spool NDEF", result);
}

}  // namespace nfc
}  // namespace filament_station
