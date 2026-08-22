#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

struct SpoolmanSpool {
  static constexpr std::uint8_t kMaximumColors = 3;
  std::uint32_t id = 0;
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
  // Bambu-Duesentemperaturbereich aus den Spoolman-Filament-Extra-Feldern
  // "bambu_temp_min"/"bambu_temp_max" (siehe SpoolmanClient::
  // decodeNumberExtraField). Nicht von Spoolman selbst vorgegeben --
  // projektspezifische Extra-Felder, die der Nutzer selbst anlegt.
  bool bambuTempFieldsPresent = false;
  bool bambuTempFieldsValid = false;
  std::uint16_t bambuTempMinC = 0;
  std::uint16_t bambuTempMaxC = 0;
};

}  // namespace models
}  // namespace filament_station
