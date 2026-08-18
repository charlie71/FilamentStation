#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kWifiPortalPasswordPrefix[] = "FS";
constexpr std::uint32_t kWifiPortalServiceIntervalMs = 20;
constexpr std::uint8_t kWifiEventQueueLength = 8;

}  // namespace filament_station::config
