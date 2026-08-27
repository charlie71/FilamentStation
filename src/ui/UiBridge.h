/**
 * @file
 * @brief The sole entry points into LVGL from tasks::uiTask(). Per "Nur
 *        UiTask greift auf LVGL zu" (only UiTask touches LVGL), every
 *        other task communicates with the UI exclusively through
 *        rtos::UiCommand/rtos::UiAction, never directly.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::ui {

/// @brief Reports LVGL's actual draw-buffer sizing/placement after initialization.
struct UiRuntimeInfo {
  std::size_t bytesPerDrawBuffer = 0;    ///< Size of a single draw buffer in bytes.
  std::size_t totalDrawBufferBytes = 0;  ///< Combined size of both draw buffers.
  bool drawBuffersInPsram = false;       ///< Whether the draw buffers were successfully allocated in PSRAM.
};

/// @brief Initializes LVGL, the display/touch drivers, and the generated UI screens.
/// @param runtimeInfo Out parameter receiving the actual draw-buffer sizing/placement.
/// @param context Owning RTOS context, used to bind UI event callbacks to the queues.
/// @return false if initialization (allocation or LVGL setup) failed.
bool initializeLvgl(UiRuntimeInfo& runtimeInfo, rtos::RtosContext& context);
/// @brief Runs pending LVGL timers/tasks for one iteration.
/// @return Milliseconds until LVGL next wants to run, for sizing the caller's sleep.
std::uint32_t runLvglTimers();
/// @brief Milliseconds since the last LVGL input event (any indev, in
///        practice only touch here) -- source signal for the power-save
///        state machine (TASKS.md Phase 11). Kept behind this wrapper so
///        LVGL access stays inside UiTask, per "Nur UiTask greift auf LVGL zu".
/// @return Milliseconds of input inactivity.
std::uint32_t inputInactiveMs();
/// @brief Applies one UiCommand to the LVGL UI (screen/widget updates).
/// @param command Command to apply.
void processUiCommand(const rtos::UiCommand& command);

}  // namespace filament_station::ui
