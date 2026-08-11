#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "config/NfcConfig.h"
#include "models/TagDefinition.h"

namespace filament_station {
namespace models {

struct RawTagData {
  TagTechnology technology = TagTechnology::Unknown;
  std::uint8_t uid[config::kNfcMaxUidLength]{};
  std::uint8_t uidLength = 0;
  std::uint8_t sak = 0;
  bool ndefPresent = false;
  bool ndefReadable = false;
  bool hardwareWritable = false;
  std::uint16_t ndefLength = 0;
  std::uint8_t ndef[config::kNfcMaxNdefBytes]{};
  // Decrypted MIFARE Classic blocks used by the documented Bambu format.
  // Bit N indicates whether block N was authenticated and read successfully.
  std::uint32_t mifareBlockMask = 0;
  std::uint8_t mifareBlocks[17][16]{};
};

struct TagReadResult {
  TagTechnology technology = TagTechnology::Unknown;
  TagFormat format = TagFormat::Unknown;
  std::uint8_t uid[config::kNfcMaxUidLength]{};
  std::uint8_t uidLength = 0;
  bool ndefPresent = false;
  bool ndefReadable = false;
  bool physicalWritableKnown = false;
  bool physicalWritable = false;
  bool payloadValid = true;
  bool writable = false;
  bool erasable = false;
  bool knownFormat = false;
  TagDefinition definition{};
};

static_assert(std::is_trivially_copyable<RawTagData>::value, "RawTagData must be trivially copyable");
static_assert(std::is_trivially_copyable<TagReadResult>::value, "TagReadResult must be trivially copyable");

}  // namespace models
}  // namespace filament_station
