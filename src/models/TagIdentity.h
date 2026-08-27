/**
 * @file
 * @brief Canonical, format-independent NFC/RFID tag identity used for
 *        Spoolman `extra.tag` matching (see docs/tag-identity.md).
 */
#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

/// @brief Which physical identifier a TagIdentity::value was derived from.
enum class TagIdentitySource : std::uint8_t {
  Unknown,    ///< No identity has been derived yet.
  NfcUid,     ///< Derived from the raw NFC UID.
  BambuUuid,  ///< Derived from the authenticated 16-byte Bambu tray UUID (block 9).
};

/// @brief Uppercase-hex tag identity, frozen once at read time and never
///        re-derived after an asynchronous step (see
///        services::normalizeTagIdentity() and docs/tag-identity.md).
struct TagIdentity {
  TagIdentitySource source = TagIdentitySource::Unknown;  ///< Origin of #value; Unknown means no valid identity.
  char value[40]{};  ///< Uppercase hex string, no separators; empty when source is Unknown.
};

static_assert(std::is_trivially_copyable<TagIdentity>::value,
              "TagIdentity must be trivially copyable");

}  // namespace models
}  // namespace filament_station
