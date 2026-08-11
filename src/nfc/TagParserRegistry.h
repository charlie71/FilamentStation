#pragma once

#include <array>
#include <cstddef>

#include "nfc/TagParsers.h"

namespace filament_station {
namespace nfc {

class TagParserRegistry {
 public:
  TagParserRegistry();
  TagParserRegistry(const ITagParser* const* parsers, std::size_t count);
  models::TagReadResult parse(const models::RawTagData& tag) const;

 private:
  FilamentStationTagParser filamentStation_{};
  BambuLabTagParser bambuLab_{};
  OpenPrintTagParser openPrintTag_{};
  OpenTag3DParser openTag3D_{};
  LegacyTagParser legacy_{};
  static constexpr std::size_t kMaximumParsers = 8;
  std::array<const ITagParser*, kMaximumParsers> parsers_{};
  std::size_t parserCount_ = 0;
};

}  // namespace nfc
}  // namespace filament_station
