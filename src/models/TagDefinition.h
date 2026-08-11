#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

enum class TagTechnology : std::uint8_t {
  Unknown,
  Ntag213,
  Ntag215,
  Ntag216,
  MifareClassic1K,
  MifareClassic4K,
  OtherIso14443A,
};

enum class TagFormat : std::uint8_t {
  Unknown,
  EmptyNdef,
  FilamentStation,
  BambuLab,
  OpenPrintTag,
  OpenTag3D,
  Legacy,
};

struct TagDefinition {
  TagFormat format = TagFormat::Unknown;
  bool hasSpoolId = false;
  std::uint32_t spoolId = 0;
  char vendor[48]{};
  char material[32]{};
  char colorName[48]{};
  char colorCode[12]{};
  float nominalFilamentWeightG = 0.0F;
  float emptySpoolWeightG = 0.0F;
  std::int16_t nozzleTempMinC = 0;
  std::int16_t nozzleTempMaxC = 0;
  char sourceDescription[64]{};
};

static_assert(std::is_trivially_copyable<TagDefinition>::value, "TagDefinition must be trivially copyable");

}  // namespace models
}  // namespace filament_station
