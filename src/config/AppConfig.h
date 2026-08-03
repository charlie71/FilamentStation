#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kApplicationName[] = "FilamentStation";
constexpr char kApplicationVersion[] = "0.1.0-dev";
constexpr std::uint32_t kCommunicationTestRequestId = 1;
constexpr std::uint32_t kUsbCdcStartupDelayMs = 2500;

}  // namespace filament_station::config
