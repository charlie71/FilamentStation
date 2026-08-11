#pragma once

#include "models/TagReadResult.h"

namespace filament_station {
namespace nfc {

enum class TagParseResult : std::uint8_t { NoMatch, Parsed, Invalid };

class ITagParser {
 public:
  virtual ~ITagParser() = default;
  virtual models::TagFormat format() const = 0;
  virtual bool canParse(const models::RawTagData& tag) const = 0;
  virtual TagParseResult parse(const models::RawTagData& tag,
                               models::TagDefinition& result) const = 0;
};

}  // namespace nfc
}  // namespace filament_station
