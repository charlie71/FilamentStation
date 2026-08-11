#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace config {

constexpr std::uint32_t kNfcScanIntervalMs = 250;
constexpr std::uint32_t kNfcTagRemovalDelayMs = 750;
constexpr std::uint32_t kPn532ResponseTimeoutMs = 500;
constexpr std::uint8_t kPn532MaxTargetRetries = 0x02;
constexpr std::size_t kNfcMaxUidLength = 10;
constexpr std::size_t kNfcMaxNdefBytes = 192;

}  // namespace config
}  // namespace filament_station
