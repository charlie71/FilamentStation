#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace services {

enum class NfcPayloadType : std::uint8_t {
  Empty,
  Spoolman,
  Bambu,
  Legacy,
  Unknown,
  Invalid,
};

struct NfcPayloadInfo {
  NfcPayloadType type = NfcPayloadType::Invalid;
  std::uint32_t spoolId = 0;
};

// Parses the Type-2 TLV area beginning at page 4.
NfcPayloadInfo parseType2Ndef(const std::uint8_t* data, std::size_t size);

// Builds a complete Type-2 NDEF TLV containing the text "spoolman:<id>".
// The output can be written page-wise beginning at page 4.
bool buildSpoolmanType2Ndef(std::uint32_t spoolId, std::uint8_t* output,
                           std::size_t capacity, std::size_t& outputSize);

}  // namespace services
}  // namespace filament_station
