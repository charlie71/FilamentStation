#pragma once

#include "nfc/ITagParser.h"

namespace filament_station {
namespace nfc {

class FilamentStationTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

class BambuLabTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

class OpenPrintTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

class OpenTag3DParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

class LegacyTagParser final : public ITagParser {
 public:
  models::TagFormat format() const override;
  bool canParse(const models::RawTagData& tag) const override;
  TagParseResult parse(const models::RawTagData& tag,
                       models::TagDefinition& result) const override;
};

}  // namespace nfc
}  // namespace filament_station
