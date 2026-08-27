/**
 * @file
 * @brief WiFiManager captive-portal and reconnect-loop tuning constants
 *        (tasks::networkTask()).
 */
#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kWifiPortalPasswordPrefix[] = "FS";       ///< Prefix for the auto-generated captive-portal WiFi password (see docs/user-guide.md).
constexpr std::uint32_t kWifiPortalServiceIntervalMs = 20;  ///< How often NetworkTask services the WiFiManager captive-portal loop.
constexpr std::uint8_t kWifiEventQueueLength = 8;         ///< Length of the internal WiFi-event queue merged into networkQueueSet.
// WiFiManager's own ESP32 auto-reconnect (WiFi_autoReconnect()) is gated
// behind the "esp32autoreconnect" build macro, which this project never
// defines -- confirmed by reading its vendored source
// (.pio/libdeps/.../WiFiManager/WiFiManager.cpp). Without it, nothing ever
// retries after a runtime WiFi loss (router reboot, signal drop, etc.); the
// device stays disconnected until a manual reboot (Robustheit/Diagnose,
// TASKS.md 10.4). NetworkTask.cpp instead retries WiFi.reconnect() itself on
// this interval whenever configured-but-disconnected and no captive portal
// is active -- long enough to not hammer the radio/AP on a genuine outage,
// short enough that a brief outage recovers well within a user's notice.
constexpr std::uint32_t kWifiReconnectIntervalMs = 15000;  ///< Interval between NetworkTask's own WiFi.reconnect() retries after a runtime connection loss.

}  // namespace filament_station::config
