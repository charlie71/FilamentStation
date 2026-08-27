/**
 * @file
 * @brief GitHub Releases OTA update source and timing constants
 *        (tasks::updateTask()).
 */
#pragma once

#include <cstdint>

namespace filament_station::config {

// Firmware-Update-Quelle (TASKS.md Phase 13.1, Nutzerentscheidung
// 2026-08-25): GitHub-Releases-API des Projekt-Repos, identisch zum
// konfigurierten git-Remote (`origin`) dieses Projekts. Kein Security-Key/
// keine Signaturpruefung geplant (siehe Phase 14.8 "kein Security-Key"),
// nur HTTPS-Transportsicherheit plus optionale SHA-256-Pruefsumme (Phase
// 13.4).
constexpr char kUpdateRepoOwner[] = "charlie71";        ///< GitHub owner of the firmware repository.
constexpr char kUpdateRepoName[] = "FilamentStation";   ///< GitHub repository name.
constexpr char kUpdateApiHost[] = "api.github.com";     ///< GitHub REST API host.
// GitHub lehnt Anfragen ohne User-Agent-Header ab.
constexpr char kUpdateUserAgent[] = "FilamentStation-OTA";  ///< User-Agent header sent with every GitHub API request.
constexpr std::uint32_t kUpdateCheckTimeoutMs = 8000;   ///< Timeout for the "check for update" HTTPS request.

// Download/Flash (TASKS.md Phase 13.3).
constexpr std::uint32_t kUpdateDownloadTimeoutMs = 15000;      ///< Connect/read timeout for the firmware download HTTPS request.
constexpr std::uint32_t kUpdateProgressReportIntervalMs = 500;  ///< Minimum interval between UpdateDownloadProgress events.
// Keine Fortschrittsbewegung innerhalb dieser Zeit gilt als haengende
// Verbindung und bricht den Download ab (Update.abort() gibt die
// Partition wieder frei).
constexpr std::uint32_t kUpdateStallTimeoutMs = 30000;  ///< Time without new data before a stalled download is aborted.

}  // namespace filament_station::config
