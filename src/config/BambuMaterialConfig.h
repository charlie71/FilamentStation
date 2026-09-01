/**
 * @file
 * @brief Paths and size limits for the SD-card-hosted Bambu material-mapping
 *        document (tasks::storageTask(), services::BambuMaterialCatalog).
 *        See docs/bambu-protocol.md.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station::config {

constexpr char kBambuMaterialsPath[] = "/config/bambu_materials.json";  ///< Active, loaded-at-boot mapping document.
// ".tmp.json"/".bak.json" suffix convention, matching
// services::JsonStorage's atomic-save scheme (see docs/storage.md) even
// though this document does not use JsonStorage's envelope/validator --
// same on-disk naming, independently implemented activation logic (see
// StorageTask.cpp), so a human inspecting the SD card sees one consistent
// pattern across every document type.
constexpr char kBambuMaterialsTempPath[] = "/config/bambu_materials.tmp.json";  ///< Download-in-progress temp file; never loaded as active.
constexpr char kBambuMaterialsBackupPath[] = "/config/bambu_materials.bak.json";  ///< Previous active file, kept until the next successful activation.
// Headroom over the schema-v2 rule catalog (the migrated default,
// pretty-printed for human editability per data/bambu-materials/README.md,
// is already ~26 KiB with 77 rules -- schema v2's per-rule match/result
// objects are considerably more verbose than schema v1's flat
// material/tray_info_idx/aliases entries) while still comfortably bounding
// a malformed/runaway download; also bounds room for future rule additions
// without another bump (Nutzerwunsch 2026-08-30, schema-v2 migration).
constexpr std::size_t kBambuMaterialsMaxFileSize = 48U * 1024U;  ///< Maximum accepted size, in bytes, of bambu_materials.json (active file or download).
// Safety-net upper bound for tasks::updateTask() waiting on
// rtos::RtosContext::bambuMaterialDownloadDone after sending Commit --
// generous headroom over the SHA-256/JSON-parse/atomic-rename work
// tasks::storageTask() actually needs for a file this size (well under a
// second in practice), so this only ever matters if something is
// genuinely stuck.
constexpr std::uint32_t kBambuMaterialCommitWaitTimeoutMs = 5000U;  ///< Maximum time UpdateTask waits for StorageTask to finish CommitBambuMaterialDownload.
// Safety net for tasks::storageTask() itself: how long an open download temp
// file may sit idle (no WriteChunk/Commit/Abort received) before StorageTask
// force-closes and discards it on its own -- covers any reason the normal
// Begin->Write->Commit/Abort sequence never completes (e.g. UpdateTask
// crashing/rebooting between Begin and Commit for an unrelated reason), not
// just the specific dropped-Commit-enqueue case UpdateTask itself now guards
// against. An open file handle surviving indefinitely (until the next
// download attempt or a full reboot) is the mechanism a 2026-08-28
// Nutzerbericht (device unbootable, SD card needed repair) traced this
// safety net back to. Generous headroom over the whole download's realistic
// duration (a few seconds even on a slow connection/card) so this only ever
// fires when something is genuinely stuck.
constexpr std::uint32_t kBambuMaterialDownloadStaleTimeoutMs = 30000U;  ///< Maximum time an open Bambu-material download temp file may sit idle before StorageTask force-closes it.

}  // namespace filament_station::config
