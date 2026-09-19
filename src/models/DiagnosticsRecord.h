/**
 * @file
 * @brief Persisted crash-diagnostics counters (/diagnostics/coredump.json,
 *        see docs/storage.md).
 */
#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

/// @brief In-memory mirror of /diagnostics/coredump.json: how many times the
///        device has booted, and a running tally of how many of those boots
///        followed a captured coredump (services::LogComponent::Rtos logs
///        the crashed task/PC/exception cause at the moment a coredump is
///        found -- this record only keeps the counters needed for the
///        Diagnose screen's "N Abstürze, letzter vor M Neustarts" summary,
///        since there is no wall-clock time source anywhere in this
///        firmware to attach a real timestamp to).
struct DiagnosticsRecord {
  std::uint32_t totalBootCount = 0;           ///< Incremented on every boot.
  std::uint32_t coredumpCount = 0;            ///< Incremented only on a boot that found a new coredump.
  std::uint32_t bootCountAtLastCoredump = 0;  ///< #totalBootCount's value at the last recorded coredump; 0 if none yet.
};

static_assert(std::is_trivially_copyable<DiagnosticsRecord>::value,
              "DiagnosticsRecord must be trivially copyable");

}  // namespace models
}  // namespace filament_station
