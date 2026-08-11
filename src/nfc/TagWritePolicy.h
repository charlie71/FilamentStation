#pragma once

#include "models/TagReadResult.h"

namespace filament_station {
namespace nfc {

inline bool mayWriteTag(const models::TagReadResult& tag) {
  return tag.format == models::TagFormat::FilamentStation && tag.writable;
}

inline bool mayEraseTag(const models::TagReadResult& tag) {
  return tag.format == models::TagFormat::FilamentStation && tag.erasable;
}

}  // namespace nfc
}  // namespace filament_station
