/**
 * @file
 * @brief Normalizes tag identifiers (NFC UIDs, Bambu tray UUIDs, and
 *        free-text input) into the canonical uppercase hex
 *        models::TagIdentity form.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "models/TagIdentity.h"

namespace filament_station {
namespace services {

/// @brief Normalizes a free-text hex identifier (as typed by a user or
///        loaded from storage) into canonical form.
/// @param input NUL-terminated hex string; separators ':', '-', and whitespace are ignored.
/// @param source Identity source to stamp on success.
/// @param identity Out parameter receiving the normalized identity.
/// @return false if `input` is null, `source` is Unknown, contains non-hex characters, is empty, or has odd length.
bool normalizeTagIdentity(const char* input,
                          models::TagIdentitySource source,
                          models::TagIdentity& identity);

/// @brief Builds a canonical identity from a raw NFC UID.
/// @param uid Raw UID bytes.
/// @param uidLength Length of `uid` in bytes.
/// @param identity Out parameter receiving the hex-encoded identity, source NfcUid.
/// @return false if `uid` is null, empty, or too long to fit the identity buffer.
bool tagIdentityFromUid(const std::uint8_t* uid, std::size_t uidLength,
                        models::TagIdentity& identity);

/// @brief Builds a canonical identity from a Bambu tray UUID given as text.
/// @param uuid NUL-terminated hex UUID string.
/// @param identity Out parameter receiving the normalized identity, source BambuUuid.
/// @return false if normalization fails or the result is not exactly 32 hex characters (16 bytes).
bool tagIdentityFromBambuUuid(const char* uuid,
                              models::TagIdentity& identity);

/// @brief Builds a canonical identity from a Bambu tray UUID given as raw bytes.
/// @param uuid Raw 16-byte UUID.
/// @param uuidLength Length of `uuid` in bytes; must be exactly 16.
/// @param identity Out parameter receiving the hex-encoded identity, source BambuUuid.
/// @return false if `uuid` is null, not 16 bytes, or is all-zero/all-0xFF (placeholder values).
bool tagIdentityFromBambuUuid(const std::uint8_t* uuid,
                              std::size_t uuidLength,
                              models::TagIdentity& identity);

}  // namespace services
}  // namespace filament_station
