#pragma once

#include <cstdint>

namespace filament_station::config {

// Energiesparen (TASKS.md Phase 11). Alle Zeiten laufen ab der letzten von
// LVGL erkannten Eingabe (lv_display_get_inactive_time()), die UiTask
// periodisch an PowerTask meldet -- siehe PowerTask.cpp.
constexpr std::uint32_t kPowerActivityReportIntervalMs = 1000;
constexpr std::uint32_t kPowerDimTimeoutMs = 30000;
constexpr std::uint32_t kPowerSleepTimeoutMs = 180000;
// Zielhelligkeit im Zustand "Gedimmt" (0-255, wie
// Light_PWM::setBrightness()). Wird ab Phase 11.2 tatsaechlich gesetzt.
constexpr std::uint8_t kPowerDimmedBrightness = 38;

// Maximale Wartezeit auf alle drei PowerDownAcknowledged-Bestaetigungen
// (Scale/Nfc/Network), bevor PowerTask trotzdem in den Light-Sleep geht --
// verhindert, dass ein haengender/verlorener Ack den Sleep fuer immer
// blockiert. Deutlich ueber dem PN532-Antwort-Timeout (kPn532ResponseTimeoutMs
// = 500ms, bis zu zwei Transceives in NfcTask::PowerDown).
constexpr std::uint32_t kPowerSleepAckTimeoutMs = 3000;
// Periodischer Sicherheitsnetz-Wake im Light-Sleep (TASKS.md 11.6): das
// FT6336-INT-Verhalten (Pegel vs. Puls, Polaritaet) ist am realen Board noch
// nicht verifiziert -- ohne dieses Sicherheitsnetz koennte ein falsch
// angenommenes Wake-Level das Geraet dauerhaft im Sleep belassen, bis zu
// einem manuellen Stromzyklus. Kurz gewaehlt, um das waehrend der ersten
// Hardware-Tests schnell sichtbar zu machen; kann spaeter vergroessert
// werden, sobald Touch-Wake verifiziert ist.
constexpr std::uint32_t kPowerSleepSafetyNetTimerMs = 30000;

}  // namespace filament_station::config
