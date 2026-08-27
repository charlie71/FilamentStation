/**
 * @file
 * @brief Minimal semantic-version parsing/comparison used by the firmware
 *        update check (TASKS.md Phase 13.1).
 */
#pragma once

#include <cstdint>

namespace filament_station {
namespace services {

/// @brief A parsed "major.minor.patch[-suffix]" version.
struct SemVer {
  std::uint32_t major = 0;  ///< Major version component.
  std::uint32_t minor = 0;  ///< Minor version component.
  std::uint32_t patch = 0;  ///< Patch version component.
  // true wenn ein "-..."/"+..."-Suffix vorhanden ist (z.B. "-dev",
  // "-rc1"). Volle Semver-Praereleasevergleiche (Identifier-fuer-
  // Identifier) sind hier bewusst nicht implementiert -- fuer
  // Firmware-Update-Zwecke reicht "Kernversion vergleichen, ein Suffix
  // zaehlt als aelter als dieselbe Kernversion ohne Suffix" (TASKS.md
  // Phase 13.1).
  bool hasSuffix = false;  ///< Whether a "-..."/"+..." suffix was present.
};

/// @brief Parses "vX.Y.Z", "X.Y.Z", or "X.Y.Z-suffix"/"X.Y.Z+meta".
/// @param text NUL-terminated version string to parse.
/// @param out Out parameter receiving the parsed version.
// Parst "vX.Y.Z", "X.Y.Z" oder "X.Y.Z-suffix"/"X.Y.Z+meta". Liefert false
// bei ungueltigem Format (kein "-"-Vorzeichen erlaubt, keine leeren
// Komponenten).
/// @return false on any invalid/malformed input.
bool parseSemVer(const char* text, SemVer& out);

/// @brief Compares two versions, treating a present suffix as older than
///        the same core version without one (e.g. 1.0.0-alpha < 1.0.0).
/// @param a First version.
/// @param b Second version.
/// @return <0 if a < b, 0 if equal, >0 if a > b.
int compareSemVer(const SemVer& a, const SemVer& b);

}  // namespace services
}  // namespace filament_station
