/**
 * @file
 * @brief Owns every concrete ITagParser and dispatches a raw scanned tag to
 *        whichever one recognizes it.
 */
#pragma once

#include <array>
#include <cstddef>

#include "nfc/TagParsers.h"

namespace filament_station {
namespace nfc {

/// @brief Tries each registered ITagParser in turn against a scanned tag
///        and assembles the final models::TagReadResult, including write
///        capabilities (nfc::updateTagCapabilities()).
class TagParserRegistry {
 public:
  /// @brief Constructs the registry with the five built-in parsers, in
  ///        their fixed try order (FilamentStation, Bambu Lab,
  ///        OpenPrintTag, OpenTag3D, legacy).
  TagParserRegistry();
  /// @brief Constructs the registry with a caller-supplied parser list.
  /// @param parsers Array of parser pointers, tried in order.
  /// @param count Number of entries in `parsers`, capped at #kMaximumParsers.
  /// @note Used by tests to inject fakes; production code uses the default constructor.
  TagParserRegistry(const ITagParser* const* parsers, std::size_t count);
  /// @brief Parses a raw scanned tag using the registered parsers.
  /// @param tag Raw tag data as read from the PN532.
  /// @return Fully assembled read result, including format, identity, and write capabilities.
  models::TagReadResult parse(const models::RawTagData& tag) const;

 private:
  FilamentStationTagParser filamentStation_{};  ///< Built-in parser for the native FilamentStation NDEF payload.
  BambuLabTagParser bambuLab_{};                ///< Built-in parser for Bambu Lab tags (NDEF marker and MIFARE public blocks).
  OpenPrintTagParser openPrintTag_{};            ///< Built-in parser for the OpenPrintTag MIME/CBOR format.
  OpenTag3DParser openTag3D_{};                  ///< Built-in parser for the OpenTag3D MIME format.
  LegacyTagParser legacy_{};                     ///< Built-in parser for the legacy plain-text spool:&lt;id&gt; payload.
  static constexpr std::size_t kMaximumParsers = 8;  ///< Upper bound on the number of parsers #parsers_ can hold.
  std::array<const ITagParser*, kMaximumParsers> parsers_{};  ///< Active parser list, tried in order by #parse().
  std::size_t parserCount_ = 0;  ///< Number of valid entries in #parsers_.
};

}  // namespace nfc
}  // namespace filament_station
