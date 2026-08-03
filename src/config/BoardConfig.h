#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr std::uint32_t kSerialBaudRate = 115200;
constexpr std::uint16_t kDisplayWidth = 480;
constexpr std::uint16_t kDisplayHeight = 320;

// Bestaetigte SPI-SD-Pinbelegung des WT32-SC01-Plus.
constexpr std::int8_t kSdChipSelectPin = 41;
constexpr std::int8_t kSdMosiPin = 40;
constexpr std::int8_t kSdClockPin = 39;
constexpr std::int8_t kSdMisoPin = 38;
constexpr bool kSdCardDetectAvailable = false;

// Ohne Card-Detect ist ein langsamer Task-Fallback erforderlich. Nach einer
// Entfernung bleibt der Storage-Fehler bis zum Neustart verriegelt.
constexpr std::uint32_t kSdHealthCheckIntervalMs = 2000;

}  // namespace filament_station::config
