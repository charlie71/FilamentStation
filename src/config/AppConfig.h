#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kApplicationName[] = "FilamentStation";
constexpr char kApplicationVersion[] = "0.1.0-dev";
constexpr std::uint32_t kCommunicationTestRequestId = 1;
constexpr std::uint32_t kUsbCdcStartupDelayMs = 5000;
constexpr std::uint32_t kUsbCdcTransmitTimeoutMs = 200;
constexpr std::uint16_t kLvglDrawBufferLines = 40;
constexpr std::uint32_t kLvglMinimumSleepMs = 1;
// Wartezeit zwischen der Best\xC3\xA4tigungsmeldung und dem tats\xC3\xA4chlichen
// ESP.restart() (TASKS.md Phase 12.1): laesst die Meldung noch sichtbar
// werden und gibt laufenden StorageTask-Schreibvorgaengen (atomicSave() ist
// bereits schreib-dann-umbenennen-atomar, Phase 2.4) Zeit zum Abschliessen.
constexpr std::uint32_t kRestartDelayMs = 1500;

}  // namespace filament_station::config
