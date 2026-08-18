#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

enum class TagIdentitySource : std::uint8_t {
  Unknown,
  NfcUid,
  BambuUuid,
};

struct TagIdentity {
  TagIdentitySource source = TagIdentitySource::Unknown;
  char value[40]{};
};

static_assert(std::is_trivially_copyable<TagIdentity>::value,
              "TagIdentity must be trivially copyable");

}  // namespace models
}  // namespace filament_station
