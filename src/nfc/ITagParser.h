/**
 * @file
 * @brief Interface implemented by every tag-format parser plugged into
 *        nfc::TagParserRegistry.
 */
#pragma once

#include "models/TagReadResult.h"

namespace filament_station {
namespace nfc {

/// @brief Outcome of one ITagParser::parse() attempt.
enum class TagParseResult : std::uint8_t {
  NoMatch,  ///< This parser does not recognize the tag's format; try the next one.
  Parsed,   ///< The tag matched this format and was decoded successfully.
  Invalid,  ///< The tag matched this format but its payload failed validation.
};

/// @brief One tag-format decoder (FilamentStation, Bambu Lab, OpenPrintTag,
///        OpenTag3D, legacy). Instances are held by nfc::TagParserRegistry
///        and tried in order for every scanned tag.
class ITagParser {
 public:
  virtual ~ITagParser() = default;
  /// @brief The tag format this parser decodes.
  /// @return Format identifier, used to tag the resulting TagDefinition.
  virtual models::TagFormat format() const = 0;
  /// @brief Cheap pre-check whether #parse() is worth attempting.
  /// @param tag Raw tag data as read from the PN532.
  /// @return true if this parser's expected marker/payload is present.
  virtual bool canParse(const models::RawTagData& tag) const = 0;
  /// @brief Decodes the tag's payload into a TagDefinition.
  /// @param tag Raw tag data as read from the PN532.
  /// @param result Out parameter receiving the decoded fields on TagParseResult::Parsed.
  /// @return Whether the tag matched, and if so, whether it was valid.
  virtual TagParseResult parse(const models::RawTagData& tag,
                               models::TagDefinition& result) const = 0;
};

}  // namespace nfc
}  // namespace filament_station
