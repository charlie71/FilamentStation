/**
 * @file
 * @brief Builds/parses the normalized Spoolman base-URL form
 *        ("protocol://host:port/basePath") used throughout configuration
 *        and storage.
 */
#pragma once

#include <cstddef>

namespace filament_station {
namespace services {

/// @brief The decomposed parts of a normalized Spoolman URL.
struct SpoolmanUrlParts {
  char protocol[8]{};   ///< "http" or "https".
  char host[64]{};      ///< Hostname or IP address, without port.
  char port[8]{};       ///< Port number as text.
  char basePath[32]{};  ///< API base path, always starting with '/'.
};

/// @brief Assembles a normalized URL string from its parts.
/// @param parts Validated URL components.
/// @param output Destination buffer receiving the assembled URL.
/// @param outputCapacity Size of `output` in bytes.
/// @return false if any part is invalid, or the result does not fit `output`.
bool buildNormalizedSpoolmanUrl(const SpoolmanUrlParts& parts, char* output,
                                std::size_t outputCapacity);
/// @brief Parses a normalized URL string into its parts.
/// @param url NUL-terminated URL to parse.
/// @param parts Out parameter receiving the decomposed components.
/// @return false if `url` is malformed or any component is invalid.
bool parseNormalizedSpoolmanUrl(const char* url, SpoolmanUrlParts& parts);

}  // namespace services
}  // namespace filament_station
