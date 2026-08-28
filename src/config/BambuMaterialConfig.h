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
// Generous headroom over any realistic material catalog (the migrated
// default is well under 10 KiB) while staying in the same size tier as
// services::JsonStorage's kLargeConfigMaxSize -- also bounds a malformed/
// runaway download.
constexpr std::size_t kBambuMaterialsMaxFileSize = 16U * 1024U;  ///< Maximum accepted size, in bytes, of bambu_materials.json (active file or download).
// Safety-net upper bound for tasks::updateTask() waiting on
// rtos::RtosContext::bambuMaterialDownloadDone after sending Commit --
// generous headroom over the SHA-256/JSON-parse/atomic-rename work
// tasks::storageTask() actually needs for a file this size (well under a
// second in practice), so this only ever matters if something is
// genuinely stuck.
constexpr std::uint32_t kBambuMaterialCommitWaitTimeoutMs = 5000U;  ///< Maximum time UpdateTask waits for StorageTask to finish CommitBambuMaterialDownload.

}  // namespace filament_station::config
