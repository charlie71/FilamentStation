#pragma once

#include <cstdint>

namespace filament_station {
namespace models {

enum class SpoolmanAppState : std::uint8_t {
  SpoolmanUnavailable,
  SpoolmanReady,
  TagFieldUnavailable,
};

constexpr SpoolmanAppState spoolmanAppState(bool serverReady,
                                             bool tagFieldReady) {
  if (!serverReady) return SpoolmanAppState::SpoolmanUnavailable;
  return tagFieldReady ? SpoolmanAppState::SpoolmanReady
                       : SpoolmanAppState::TagFieldUnavailable;
}

constexpr bool spoolmanOperationsAvailable(SpoolmanAppState state) {
  return state != SpoolmanAppState::SpoolmanUnavailable;
}

constexpr bool spoolmanTagOperationsAvailable(SpoolmanAppState state) {
  return state == SpoolmanAppState::SpoolmanReady;
}

}  // namespace models
}  // namespace filament_station
