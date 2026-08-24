#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

struct SpoolmanSpool {
  static constexpr std::uint8_t kMaximumColors = 3;
  std::uint32_t id = 0;
  // The filament this spool is wound from -- bambu_temp_min/bambu_temp_max/
  // flow_dynamics_k_factor are properties *of the filament*, not the spool (per
  // Nutzerhinweis 2026-08-24), so they live on SpoolmanFilament instead and
  // are fetched via a dedicated SpoolmanCommandType::LoadFilament request
  // using this id, rather than trusted from the spool response's nested
  // (and not necessarily complete) filament object. See
  // docs/bambu-protocol.md.
  std::uint32_t filamentId = 0;
  char vendor[32]{};
  char filament[40]{};
  char material[24]{};
  char extraTag[40]{};
  bool extraTagPresent = false;
  bool extraTagValid = false;
  char colorHex[kMaximumColors][9]{};
  std::uint8_t colorCount = 0;
  float initialWeightGrams = 0.0F;
  float emptyWeightGrams = 0.0F;
  float remainingWeightGrams = 0.0F;
  bool archived = false;
};

}  // namespace models
}  // namespace filament_station
