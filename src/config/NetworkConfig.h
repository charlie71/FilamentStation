#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kWifiPortalSsidPrefix[] = "FilamentStation";
constexpr char kWifiPortalPasswordPrefix[] = "FS";
constexpr std::uint32_t kWifiPortalTimeoutSeconds = 180;
constexpr std::uint32_t kWifiConnectTimeoutSeconds = 20;
constexpr std::uint32_t kWifiPortalServiceIntervalMs = 20;
constexpr std::uint8_t kWifiEventQueueLength = 8;

}  // namespace filament_station::config
