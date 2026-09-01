/**
 * @file
 * @brief Identifies NTAG213/215/216 chips from their GET_VERSION/capability
 *        response and checks whether a page range is safely writable.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "models/TagDefinition.h"

namespace filament_station {
namespace services {

/// @brief Identifies the specific NTAG21x variant from its ISO14443 GET_VERSION
///        response and capability container (CC) bytes.
/// @param version Raw 8-byte GET_VERSION response.
/// @param versionLength Length of `version` in bytes; must be exactly 8.
/// @param capability 4-byte capability container from page 3, or null if unformatted/unknown.
/// @return The identified NTAG21x variant, or models::TagTechnology::OtherIso14443A if not recognized.
models::TagTechnology identifyNtag21x(const std::uint8_t* version,
                                      std::size_t versionLength,
                                      const std::uint8_t* capability);

/// @brief Checks whether a page range on an NTAG21x is safe to write to
///        without password protection or locked pages interfering.
/// @param technology Identified chip variant; only NTAG213/215/216 are supported.
/// @param lastPage Highest page index that will be written.
/// @param capability 4-byte capability container from page 3.
/// @param staticLocks 2-byte static lock bits from page 2.
/// @param dynamicLocks 3-byte dynamic lock bits.
/// @param auth0 Page number at/above which password authentication is required.
/// @return true if the range can be written without hitting a lock or password gate.
bool ntag21xRangeWritable(models::TagTechnology technology,
                          std::uint8_t lastPage,
                          const std::uint8_t* capability,
                          const std::uint8_t* staticLocks,
                          const std::uint8_t* dynamicLocks,
                          std::uint8_t auth0);

}  // namespace services
}  // namespace filament_station
