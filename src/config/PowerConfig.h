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

}  // namespace filament_station::config
