/**
 * @file
 * @brief The five built-in ITagParser implementations registered by
 *        nfc::TagParserRegistry.
 */
#pragma once

#include "nfc/ITagParser.h"

namespace filament_station {
namespace nfc {

/// @brief Decodes the native FilamentStation NDEF payload (`spoolman:<id>`).
class FilamentStationTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

/// @brief Decodes Bambu Lab tags: either the NDEF-embedded marker, or the
///        vendor's MIFARE Classic 1K public-block layout.
class BambuLabTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

/// @brief Decodes the third-party OpenPrintTag MIME/CBOR NDEF format.
class OpenPrintTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

/// @brief Decodes the third-party OpenTag3D MIME NDEF format.
class OpenTag3DParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

/// @brief Decodes the legacy plain-text `spool:<id>` NDEF payload used
///        before the FilamentStation format existed.
class LegacyTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

}  // namespace nfc
}  // namespace filament_station
