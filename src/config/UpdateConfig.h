#pragma once

namespace filament_station::config {

// Firmware-Update-Quelle (TASKS.md Phase 13.1, Nutzerentscheidung
// 2026-08-25): GitHub-Releases-API des Projekt-Repos, identisch zum
// konfigurierten git-Remote (`origin`) dieses Projekts. Kein Security-Key/
// keine Signaturpruefung geplant (siehe Phase 14.8 "kein Security-Key"),
// nur HTTPS-Transportsicherheit plus optionale SHA-256-Pruefsumme (Phase
// 13.4).
constexpr char kUpdateRepoOwner[] = "charlie71";
constexpr char kUpdateRepoName[] = "FilamentStation";
constexpr char kUpdateApiHost[] = "api.github.com";

}  // namespace filament_station::config
