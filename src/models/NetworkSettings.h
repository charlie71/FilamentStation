/**
 * @file
 * @brief Persisted WiFi/network configuration (/config/network.json).
 */
#pragma once

#include <cstdint>

namespace filament_station::models {

/// @brief WiFi and captive-portal configuration, as loaded from
///        /config/network.json and applied by tasks::networkTask().
struct NetworkSettings {
  char hostname[33]{};    ///< DHCP/mDNS hostname advertised by the device.
  bool dhcp = true;       ///< Whether to use DHCP (true) or the static fields below (false).
  char ipAddress[16]{};   ///< Static IP address, used only if #dhcp is false.
  char gateway[16]{};     ///< Static gateway, used only if #dhcp is false.
  char subnetMask[16]{};  ///< Static subnet mask, used only if #dhcp is false.
  char dns[16]{};         ///< Static DNS server, used only if #dhcp is false.
  char portalName[33]{};  ///< Base name for the auto-generated captive-portal SSID.
  std::uint16_t portalTimeoutSeconds = 180;   ///< How long the captive portal stays open before giving up.
  std::uint16_t connectTimeoutSeconds = 20;   ///< Timeout for a single WiFi connect attempt.
};

}  // namespace filament_station::models
