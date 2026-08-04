#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kApplicationName[] = "FilamentStation";
constexpr char kApplicationVersion[] = "0.1.0-dev";
constexpr std::uint32_t kCommunicationTestRequestId = 1;
constexpr std::uint32_t kUsbCdcStartupDelayMs = 5000;
constexpr std::uint32_t kUsbCdcTransmitTimeoutMs = 200;
constexpr std::uint16_t kLvglDrawBufferLines = 40;
constexpr std::uint32_t kLvglHandlerPeriodMs = 10;

}  // namespace filament_station::config
