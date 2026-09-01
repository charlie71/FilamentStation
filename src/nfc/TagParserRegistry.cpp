/**
 * @file
 * @brief Implements nfc::TagParserRegistry: parser dispatch and the shared
 *        early-out handling for empty/unknown tags.
 */
#include "nfc/TagParserRegistry.h"

#include <cstring>

#include "nfc/TagWritePolicy.h"
#include "services/NfcPayload.h"
#include "services/TagIdentity.h"

namespace filament_station {
namespace nfc {

namespace {
/// @brief Fills in TagReadResult::capabilities before returning a result.
/// @param result Result assembled so far.
/// @return `result` with nfc::updateTagCapabilities() applied.
models::TagReadResult finalize(models::TagReadResult result) {
  updateTagCapabilities(result);
  return result;
}
}  // namespace

TagParserRegistry::TagParserRegistry()
    : parsers_{{&filamentStation_, &bambuLab_, &openPrintTag_, &openTag3D_,
                &legacy_}},
      parserCount_(5) {}

TagParserRegistry::TagParserRegistry(const ITagParser* const* parsers,
                                     std::size_t count)
    : parserCount_(count > kMaximumParsers ? kMaximumParsers : count) {
  for (std::size_t index = 0; index < parserCount_; ++index) {
    parsers_[index] = parsers[index];
  }
}

models::TagReadResult TagParserRegistry::parse(
    const models::RawTagData& tag) const {
  models::TagReadResult result{};
  result.technology = tag.technology;
  result.uidLength = tag.uidLength;
  std::memcpy(result.uid, tag.uid, tag.uidLength);
  services::tagIdentityFromUid(tag.uid, tag.uidLength, result.identity);
  result.ndefPresent = tag.ndefPresent;
  result.ndefReadable = tag.ndefReadable;
  result.physicalWritableKnown = tag.hardwareWritableKnown;
  result.physicalWritable =
      result.physicalWritableKnown && tag.hardwareWritable;

  if (!tag.ndefPresent && result.physicalWritable) {
    result.format = models::TagFormat::EmptyNdef;
    result.knownFormat = true;
    result.writable = true;
    result.erasable = true;
    result.definition.format = models::TagFormat::EmptyNdef;
    return finalize(result);
  }

  for (std::size_t index = 0; index < parserCount_; ++index) {
    const ITagParser* parser = parsers_[index];
    if (parser == nullptr) continue;
    if (!parser->canParse(tag)) continue;
    models::TagDefinition definition{};
    const TagParseResult parseResult = parser->parse(tag, definition);
    if (parseResult == TagParseResult::Invalid) {
      result.format = parser->format();
      result.knownFormat = true;
      result.payloadValid = false;
      result.writable = false;
      result.erasable = false;
      return finalize(result);
    }
    if (parseResult != TagParseResult::Parsed) continue;
    result.format = parser->format();
    result.knownFormat = true;
    result.definition = definition;
    // Block 9 is the authenticated 16-byte Bambu tray UID. Prefer it as the
    // stable identity; retain the NFC UID when that optional block is absent
    // or contains no usable identifier.
    if (result.format == models::TagFormat::BambuLab &&
        (tag.mifareBlockMask & (1UL << 9)) != 0U) {
      models::TagIdentity bambuIdentity{};
      if (services::tagIdentityFromBambuUuid(tag.mifareBlocks[9], 16U,
                                             bambuIdentity)) {
        result.identity = bambuIdentity;
      }
    }
    // Only native FilamentStation data may inherit the physical tag's write
    // capability. Every foreign format is read-only in version 1.
    result.writable =
        (result.format == models::TagFormat::FilamentStation ||
         result.format == models::TagFormat::Legacy) &&
        result.physicalWritable;
    result.erasable = result.writable;
    return finalize(result);
  }

  if (tag.ndefPresent && tag.ndefReadable &&
      services::parseType2Ndef(tag.ndef, tag.ndefLength).type ==
          services::NfcPayloadType::Empty) {
    result.format = models::TagFormat::EmptyNdef;
    result.knownFormat = true;
    result.writable = tag.hardwareWritable;
    result.erasable = tag.hardwareWritable;
    result.definition.format = models::TagFormat::EmptyNdef;
    return finalize(result);
  }

  if (tag.ndefPresent && tag.ndefReadable &&
      services::parseType2Ndef(tag.ndef, tag.ndefLength).type ==
          services::NfcPayloadType::Invalid) {
    result.payloadValid = false;
  }

  // Unknown data never inherits a physical write capability.
  result.format = models::TagFormat::Unknown;
  result.writable = false;
  result.erasable = false;
  return finalize(result);
}

}  // namespace nfc
}  // namespace filament_station
