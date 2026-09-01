/**
 * @file
 * @brief Timing and brightness constants for the Active/Dimmed/Sleep
 *        power-saving state machine (see tasks::powerTask(),
 *        tasks::PowerState).
 */
#pragma once

#include <cstdint>

namespace filament_station::config {

// Energiesparen (TASKS.md Phase 11). Alle Zeiten laufen ab der letzten von
// LVGL erkannten Eingabe (lv_display_get_inactive_time()), die UiTask
// periodisch an PowerTask meldet -- siehe PowerTask.cpp. Dieses Intervall
// begrenzt zugleich UiTasks maximale Wartezeit auf der uiCommandQueue
// (boundedSleepMs, UiTask.cpp) -- bei voelliger LVGL-Ruhe (gedimmter,
// unbewegter Screen) wird ein Touch erst beim naechsten Schleifendurchlauf
// ueberhaupt bemerkt (kein eigener Touch-Interrupt, siehe docs/rtos.md).
// War bei 1000 ms gesetzt, wodurch das Aufhellen aus "Gedimmt" je nach
// Zeitpunkt des Touches gegenueber dem laufenden Report-/Wartezyklus bis zu
// ca. 1,5 s dauern konnte (Nutzerbericht 2026-08-27) -- auf 150 ms gesenkt,
// ohne Auswirkung auf den echten Light-Sleep-Stromverbrauch (der laeuft
// weiterhin ueber den separaten GPIO-Wake in PowerTask::
// sleepUntilTouchWake(), nicht ueber dieses Intervall).
constexpr std::uint32_t kPowerActivityReportIntervalMs = 150;  ///< How often UiTask reports input inactivity to PowerTask; also bounds UiTask's own queue-wait latency.
constexpr std::uint32_t kPowerDimTimeoutMs = 30000;    ///< Inactivity duration after which PowerState transitions Active -> Dimmed.
constexpr std::uint32_t kPowerSleepTimeoutMs = 180000;  ///< Inactivity duration after which PowerState transitions Dimmed -> Sleep.
// Zielhelligkeit im Zustand "Gedimmt" (0-255, wie
// Light_PWM::setBrightness()). Wird ab Phase 11.2 tatsaechlich gesetzt.
constexpr std::uint8_t kPowerDimmedBrightness = 28;  ///< Backlight duty cycle (0-255) while PowerState is Dimmed.

// Maximale Wartezeit auf alle drei PowerDownAcknowledged-Bestaetigungen
// (Scale/Nfc/Network), bevor PowerTask trotzdem in den Light-Sleep geht --
// verhindert, dass ein haengender/verlorener Ack den Sleep fuer immer
// blockiert. Deutlich ueber dem PN532-Antwort-Timeout (kPn532ResponseTimeoutMs
// = 500ms, bis zu zwei Transceives in NfcTask::PowerDown).
constexpr std::uint32_t kPowerSleepAckTimeoutMs = 3000;  ///< Maximum time PowerTask waits for all three PowerDownAcknowledged replies before entering light sleep anyway.
// Periodischer Sicherheitsnetz-Wake im Light-Sleep (TASKS.md 11.6): das
// FT6336-INT-Verhalten (Pegel vs. Puls, Polaritaet) ist am realen Board noch
// nicht verifiziert -- ohne dieses Sicherheitsnetz koennte ein falsch
// angenommenes Wake-Level das Geraet dauerhaft im Sleep belassen, bis zu
// einem manuellen Stromzyklus. Kurz gewaehlt, um das waehrend der ersten
// Hardware-Tests schnell sichtbar zu machen; kann spaeter vergroessert
// werden, sobald Touch-Wake verifiziert ist.
constexpr std::uint32_t kPowerSleepSafetyNetTimerMs = 30000;  ///< Periodic timer wake used as a safety net in case the GPIO touch-wake assumption is wrong.

}  // namespace filament_station::config
