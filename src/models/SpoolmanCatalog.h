/**
 * @file
 * @brief Normalized Spoolman vendor/filament catalog data and the
 *        import/weight-update request shapes built from it.
 */
#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

/// @brief One Spoolman vendor/manufacturer entry.
struct SpoolmanVendor {
  std::uint32_t id = 0;                  ///< Spoolman vendor id.
  char name[65]{};                       ///< Vendor/manufacturer name.
  float emptySpoolWeightGrams = 0.0F;    ///< Vendor-level default empty spool weight, if known.
};

/// @brief One Spoolman filament (material) catalog entry.
struct SpoolmanFilament {
  std::uint32_t id = 0;         ///< Spoolman filament id.
  std::uint32_t vendorId = 0;   ///< Owning SpoolmanVendor id.
  char name[65]{};               ///< Filament product name.
  char material[65]{};           ///< Material family (e.g. "PLA").
  char colorHex[9]{};            ///< Color as 8-digit RRGGBBAA hex.
  float densityGramsPerCm3 = 0.0F;   ///< Material density.
  float diameterMillimeters = 0.0F;  ///< Filament diameter.
  float weightGrams = 0.0F;          ///< Nominal net filament weight.
  float emptySpoolWeightGrams = 0.0F;  ///< Default empty spool weight for this filament.
  std::int16_t nozzleTemperatureC = 0;  ///< Recommended nozzle temperature.
  std::int16_t bedTemperatureC = 0;     ///< Recommended bed temperature.
  // Bambu-Duesentemperaturbereich aus den projektspezifischen Spoolman-
  // Filament-Extra-Feldern "bambu_temp_min"/"bambu_temp_max" (kein
  // Spoolman-Standardfeld, vom Nutzer selbst angelegt) -- eine
  // Materialeigenschaft, siehe docs/bambu-protocol.md. Beide Felder muessen
  // vorhanden und als Zahl > 0 dekodierbar sein, sonst bleibt
  // bambuTempFieldsValid false und der Aufrufer zeigt einen Hinweis statt
  // eine erfundene Temperatur zu senden.
  bool bambuTempFieldsPresent = false;  ///< Whether the bambu_temp_min/max extra fields exist.
  bool bambuTempFieldsValid = false;    ///< Whether both extra fields decoded as valid positive numbers.
  std::uint16_t bambuTempMinC = 0;      ///< Decoded bambu_temp_min, only valid if #bambuTempFieldsValid.
  std::uint16_t bambuTempMaxC = 0;      ///< Decoded bambu_temp_max, only valid if #bambuTempFieldsValid.
  // Bambu-K-Faktor (Flow-Dynamics-Kalibrierung) aus dem Extra-Feld
  // "flow_dynamics_k_factor" -- Anzeige-only (Nutzerwunsch 2026-08-24), kein
  // Einfluss auf ein an den Drucker gesendetes Kommando.
  bool bambuKFactorPresent = false;  ///< Whether the flow_dynamics_k_factor extra field exists.
  bool bambuKFactorValid = false;    ///< Whether it decoded as a valid number.
  float bambuKFactor = 0.0F;         ///< Decoded K-factor, display-only, only valid if #bambuKFactorValid.
};

/// @brief Vendor+filament+weight combination used to create a new Spoolman
///        spool from an imported tag definition.
struct SpoolmanImportDefinition {
  SpoolmanVendor vendor{};        ///< Vendor to find-or-create.
  SpoolmanFilament filament{};    ///< Filament to find-or-create.
  float initialWeightGrams = 0.0F;    ///< Initial (full) filament weight for the new spool.
  float emptySpoolWeightGrams = 0.0F;  ///< Empty spool weight for the new spool.
};

/// @brief Request to update one spool's weight fields in Spoolman.
struct SpoolmanWeightUpdate {
  std::uint32_t spoolId = 0;              ///< Spool to update.
  float remainingWeightGrams = 0.0F;      ///< New remaining weight.
  float initialWeightGrams = 0.0F;        ///< New initial weight, only applied if #updateInitialWeight.
  float emptySpoolWeightGrams = 0.0F;     ///< New empty spool weight, only applied if #updateEmptySpoolWeight.
  bool updateInitialWeight = false;       ///< Whether #initialWeightGrams should be written.
  bool updateEmptySpoolWeight = false;    ///< Whether #emptySpoolWeightGrams should be written.
};

}  // namespace models
}  // namespace filament_station
