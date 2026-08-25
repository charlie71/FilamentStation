#pragma once

#include <cstdint>

namespace filament_station {
namespace services {

struct SemVer {
  std::uint32_t major = 0;
  std::uint32_t minor = 0;
  std::uint32_t patch = 0;
  // true wenn ein "-..."/"+..."-Suffix vorhanden ist (z.B. "-dev",
  // "-rc1"). Volle Semver-Praereleasevergleiche (Identifier-fuer-
  // Identifier) sind hier bewusst nicht implementiert -- fuer
  // Firmware-Update-Zwecke reicht "Kernversion vergleichen, ein Suffix
  // zaehlt als aelter als dieselbe Kernversion ohne Suffix" (TASKS.md
  // Phase 13.1).
  bool hasSuffix = false;
};

// Parst "vX.Y.Z", "X.Y.Z" oder "X.Y.Z-suffix"/"X.Y.Z+meta". Liefert false
// bei ungueltigem Format (kein "-"-Vorzeichen erlaubt, keine leeren
// Komponenten).
bool parseSemVer(const char* text, SemVer& out);

// <0 wenn a < b, 0 wenn gleich, >0 wenn a > b. Vergleicht zuerst
// major/minor/patch, bei Gleichstand gilt "hat Suffix" als aelter (passend
// zu Semver: 1.0.0-alpha < 1.0.0).
int compareSemVer(const SemVer& a, const SemVer& b);

}  // namespace services
}  // namespace filament_station
