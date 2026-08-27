/**
 * @file
 * @brief Unused early-scaffolding placeholder; superseded by AppTask's
 *        own richer runtime state (currentSpoolmanAppState,
 *        models::PrinterStateCollection, and the various Pending* state
 *        machines in tasks/AppTask.cpp). No code references this file.
 */
#pragma once

namespace filament_station::app {
/// @deprecated Unused; AppTask tracks its runtime state directly instead of through this type.
enum class ApplicationState { Starting, Ready, Error };
}
