/**
 * @file
 * @brief Normalized representation of one Spoolman spool, as parsed from
 *        the Spoolman REST API (services::SpoolmanClient).
 */
#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

/// @brief One Spoolman spool's data, normalized for use across task
///        boundaries.
struct SpoolmanSpool {
  static constexpr std::uint8_t kMaximumColors = 3;  ///< Largest number of colors #colorHex can hold.
  std::uint32_t id = 0;  ///< Spoolman spool id.
  // The filament this spool is wound from -- bambu_temp_min/bambu_temp_max/
  // flow_dynamics_k_factor are properties *of the filament*, not the spool (per
  // Nutzerhinweis 2026-08-24), so they live on SpoolmanFilament instead and
  // are fetched via a dedicated SpoolmanCommandType::LoadFilament request
  // using this id, rather than trusted from the spool response's nested
  // (and not necessarily complete) filament object. See
  // docs/bambu-protocol.md.
  std::uint32_t filamentId = 0;   ///< Id of the filament this spool is wound from (see models::SpoolmanFilament).
  char vendor[32]{};              ///< Filament vendor/manufacturer name.
  char filament[40]{};            ///< Filament product name.
  char material[24]{};            ///< Material family (e.g. "PLA").
  char extraTag[40]{};            ///< Decoded `extra.tag` value (NFC identity association), if present.
  bool extraTagPresent = false;   ///< Whether the `extra.tag` field exists on this spool.
  bool extraTagValid = false;     ///< Whether `extra.tag` decoded successfully as a valid tag identity.
  char colorHex[kMaximumColors][9]{};  ///< Up to kMaximumColors colors as 8-digit RRGGBBAA hex strings.
  std::uint8_t colorCount = 0;    ///< Number of valid entries in #colorHex.
  float initialWeightGrams = 0.0F;    ///< Initial (full) filament weight in grams.
  float emptyWeightGrams = 0.0F;      ///< Empty spool weight in grams.
  float remainingWeightGrams = 0.0F;  ///< Current remaining filament weight in grams.
  bool archived = false;          ///< Whether the spool is archived in Spoolman.
};

}  // namespace models
}  // namespace filament_station
