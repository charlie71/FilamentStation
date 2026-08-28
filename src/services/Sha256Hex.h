/**
 * @file
 * @brief Pure SHA-256 hex-digest text parsing, shared by tasks::updateTask()
 *        (checksum-sidecar fetch) and tasks::storageTask() (Bambu
 *        material-mapping download activation). No network/storage access
 *        here -- callers fetch/read the text, this only parses it.
 */
#pragma once

namespace filament_station {
namespace services {

/// @brief Extracts a lowercase 64-hex-digit SHA-256 digest from the start of
///        a checksum-file body.
/// @param text Checksum text (may have " filename" trailing, like
///        `sha256sum` output -- everything after the first 64 hex digits is
///        ignored).
/// @param out Destination buffer, at least 65 bytes (64 hex digits + NUL).
/// @return false if `text` is null or does not start with 64 hex digits.
bool extractHexSha256(const char* text, char* out);

}  // namespace services
}  // namespace filament_station
