#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

struct SpoolmanVendor {
  std::uint32_t id = 0;
  char name[65]{};
  float emptySpoolWeightGrams = 0.0F;
};

struct SpoolmanFilament {
  std::uint32_t id = 0;
  std::uint32_t vendorId = 0;
  char name[65]{};
  char material[65]{};
  char colorHex[9]{};
  float densityGramsPerCm3 = 0.0F;
  float diameterMillimeters = 0.0F;
  float weightGrams = 0.0F;
  float emptySpoolWeightGrams = 0.0F;
  std::int16_t nozzleTemperatureC = 0;
  std::int16_t bedTemperatureC = 0;
  // Bambu-Duesentemperaturbereich aus den projektspezifischen Spoolman-
  // Filament-Extra-Feldern "bambu_temp_min"/"bambu_temp_max" (kein
  // Spoolman-Standardfeld, vom Nutzer selbst angelegt) -- eine
  // Materialeigenschaft, siehe docs/bambu-protocol.md. Beide Felder muessen
  // vorhanden und als Zahl > 0 dekodierbar sein, sonst bleibt
  // bambuTempFieldsValid false und der Aufrufer zeigt einen Hinweis statt
  // eine erfundene Temperatur zu senden.
  bool bambuTempFieldsPresent = false;
  bool bambuTempFieldsValid = false;
  std::uint16_t bambuTempMinC = 0;
  std::uint16_t bambuTempMaxC = 0;
  // Bambu-K-Faktor (Flow-Dynamics-Kalibrierung) aus dem Extra-Feld
  // "flow_dynamics_k_factor" -- Anzeige-only (Nutzerwunsch 2026-08-24), kein
  // Einfluss auf ein an den Drucker gesendetes Kommando.
  bool bambuKFactorPresent = false;
  bool bambuKFactorValid = false;
  float bambuKFactor = 0.0F;
};

struct SpoolmanImportDefinition {
  SpoolmanVendor vendor{};
  SpoolmanFilament filament{};
  float initialWeightGrams = 0.0F;
  float emptySpoolWeightGrams = 0.0F;
};

struct SpoolmanWeightUpdate {
  std::uint32_t spoolId = 0;
  float remainingWeightGrams = 0.0F;
  float initialWeightGrams = 0.0F;
  float emptySpoolWeightGrams = 0.0F;
  bool updateInitialWeight = false;
  bool updateEmptySpoolWeight = false;
};

}  // namespace models
}  // namespace filament_station
