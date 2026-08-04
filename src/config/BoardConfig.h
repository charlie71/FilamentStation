#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr std::uint32_t kSerialBaudRate = 115200;
constexpr std::uint16_t kDisplayWidth = 480;
constexpr std::uint16_t kDisplayHeight = 320;

// WT32-SC01-Plus LCD: ST7796UI, 8-bit MCU8080 parallel interface.
constexpr std::int8_t kDisplayBacklightPin = 45;
constexpr bool kDisplayBacklightActiveHigh = true;
constexpr std::int8_t kDisplayResetPin = 4;
constexpr std::int8_t kDisplayCommandDataPin = 0;
constexpr std::int8_t kDisplayWritePin = 47;
constexpr std::int8_t kDisplayTearingEffectPin = 48;
constexpr std::int8_t kDisplayDataPins[8] = {9, 46, 3, 8, 18, 17, 16, 15};

// WT32-SC01-Plus touch: FT6336U on I2C. Reset is shared with LCD.
constexpr std::int8_t kTouchInterruptPin = 7;
constexpr std::int8_t kTouchSdaPin = 6;
constexpr std::int8_t kTouchSclPin = 5;
constexpr std::int8_t kTouchResetPin = kDisplayResetPin;

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
