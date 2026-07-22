#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr std::uint32_t kSerialBaudRate = 115200;
constexpr std::uint16_t kDisplayWidth = 480;
constexpr std::uint16_t kDisplayHeight = 320;

// GPIOs bleiben bis zur Verifikation der konkreten Boardrevision undefiniert.

}  // namespace filament_station::config

