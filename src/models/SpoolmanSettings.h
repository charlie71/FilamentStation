/**
 * @file
 * @brief Persisted Spoolman server connection configuration
 *        (/config/spoolman.json).
 */
#pragma once

#include <cstdint>

namespace filament_station::models {

/// @brief Spoolman server connection settings, as loaded from
///        /config/spoolman.json. #serverUrl is the already-normalized
///        base URL (see services::SpoolmanUrl for parsing/building it
///        from the individual protocol/host/port/basePath fields shown
///        in the UI).
struct SpoolmanSettings {
  bool enabled = false;         ///< Whether Spoolman integration is enabled.
  char name[32]{};              ///< Display name for this server connection.
  char serverUrl[128]{};        ///< Normalized base URL, e.g. "http://spoolman.local:7912/api/v1".
  std::uint32_t timeoutMs = 5000;  ///< HTTP request timeout.
};

}  // namespace filament_station::models
