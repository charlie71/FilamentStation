/**
 * @file
 * @brief Spoolman connection/tag-field readiness state, derived by
 *        tasks::appTask() from two FreeRTOS event-group bits.
 */
#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

/**
 * @brief Spoolman readiness as seen by the rest of the application.
 *
 * Recomputed by spoolmanAppState() from EVENT_SPOOLMAN_READY and
 * EVENT_SPOOLMAN_TAG_FIELD_READY (see rtos::Events.h and
 * docs/architecture.md) whenever either bit changes -- not a
 * self-transitioning state machine, but a pure function of the two
 * underlying readiness signals.
 *
 * @dot
 * digraph SpoolmanAppState {
 *   rankdir=LR;
 *   SpoolmanUnavailable -> TagFieldUnavailable [label="server reachable"];
 *   TagFieldUnavailable -> SpoolmanReady [label="extra.tag field ready"];
 *   SpoolmanReady -> TagFieldUnavailable [label="extra.tag field lost"];
 *   TagFieldUnavailable -> SpoolmanUnavailable [label="server/WiFi lost"];
 *   SpoolmanReady -> SpoolmanUnavailable [label="server/WiFi lost"];
 * }
 * @enddot
 */
enum class SpoolmanAppState : std::uint8_t {
  SpoolmanUnavailable,  ///< Server not reachable; all Spoolman-dependent actions are locked.
  SpoolmanReady,        ///< Server reachable and extra.tag field usable; all actions available.
  TagFieldUnavailable,  ///< Server reachable, but extra.tag is missing/incompatible; tag assignment is locked, other actions remain available.
};

/// @brief Derives the current SpoolmanAppState from the two underlying
///        readiness signals.
/// @param serverReady Whether the Spoolman server responded to a health check.
/// @param tagFieldReady Whether the Spoolman `extra.tag` field exists and is usable.
/// @return The resulting SpoolmanAppState.
constexpr SpoolmanAppState spoolmanAppState(bool serverReady,
                                             bool tagFieldReady) {
  if (!serverReady) return SpoolmanAppState::SpoolmanUnavailable;
  return tagFieldReady ? SpoolmanAppState::SpoolmanReady
                       : SpoolmanAppState::TagFieldUnavailable;
}

/// @brief Whether any online Spoolman operation (search, weight update, ...)
///        is currently permitted.
/// @param state Current SpoolmanAppState.
/// @return True unless state is SpoolmanUnavailable.
constexpr bool spoolmanOperationsAvailable(SpoolmanAppState state) {
  return state != SpoolmanAppState::SpoolmanUnavailable;
}

/// @brief Whether NFC tag assignment/removal (which requires a usable
///        extra.tag field) is currently permitted.
/// @param state Current SpoolmanAppState.
/// @return True only when state is SpoolmanReady.
constexpr bool spoolmanTagOperationsAvailable(SpoolmanAppState state) {
  return state == SpoolmanAppState::SpoolmanReady;
}

}  // namespace models
}  // namespace filament_station
