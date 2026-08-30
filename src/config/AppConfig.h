/**
 * @file
 * @brief General application identity and startup/shutdown timing
 *        constants used across multiple tasks.
 */
#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kApplicationName[] = "FilamentStation";  ///< Product name shown on the boot screen and in logs.
constexpr char kApplicationVersion[] = "0.2.0";      ///< Current firmware version; source of truth for OTA update comparisons (see services::SemVer).
constexpr std::uint32_t kCommunicationTestRequestId = 1;  ///< Fixed requestId for the boot-time UI<->AppTask round-trip self-test.
constexpr std::uint32_t kUsbCdcStartupDelayMs = 5000;    ///< Delay after Serial.begin() before startup diagnostics are logged, giving the host time to enumerate the native USB-CDC device.
constexpr std::uint32_t kUsbCdcTransmitTimeoutMs = 200;  ///< Serial.setTxTimeoutMs() value; bounds how long a log write can block if the host isn't reading.
constexpr std::uint16_t kLvglDrawBufferLines = 40;       ///< Height (in pixels) of each of LVGL's two partial PSRAM draw buffers.
constexpr std::uint32_t kLvglMinimumSleepMs = 1;         ///< Floor applied to LVGL's requested timer delay so UiTask never busy-waits on a zero-length wait.

// Wartezeit zwischen der Best\xC3\xA4tigungsmeldung und dem tats\xC3\xA4chlichen
// ESP.restart() (TASKS.md Phase 12.1): laesst die Meldung noch sichtbar
// werden und gibt laufenden StorageTask-Schreibvorgaengen (atomicSave() ist
// bereits schreib-dann-umbenennen-atomar, Phase 2.4) Zeit zum Abschliessen.
constexpr std::uint32_t kRestartDelayMs = 1500;  ///< Delay between the restart confirmation dialog and the actual ESP.restart() call.

}  // namespace filament_station::config
