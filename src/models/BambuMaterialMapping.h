/**
 * @file
 * @brief In-RAM Bambu AMS material-mapping table, loaded at runtime from
 *        /config/bambu_materials.json (see services/BambuMaterialCatalog.h
 *        and docs/bambu-protocol.md). Replaces the previous compiled-in
 *        constexpr table so the mapping can be extended without a firmware
 *        rebuild.
 */
#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

// Sized for headroom over the ~50 materials/support-materials known at the
// time this was introduced (2026-08-28), not a theoretical worst case --
// loadBambuMaterialCatalog()/parseBambuMaterialCatalog() reject a file with
// more entries than this rather than silently truncating it.
constexpr std::size_t kMaxBambuMaterialMappings = 96;    ///< Maximum number of entries loadable from bambu_materials.json.
constexpr std::size_t kBambuMaterialFieldLength = 24;    ///< Buffer size for BambuMaterialMappingEntry::material/trayType.
constexpr std::size_t kBambuTrayInfoIdxLength = 12;      ///< Buffer size for BambuMaterialMappingEntry::trayInfoIdx (e.g. "GFL99").
// Aliases are stored as one '|'-separated string rather than a fixed
// char[N][M] array: most entries have 0-4 short aliases, so a 2D array would
// mostly be padding. resolveBambuMaterial() walks the tokens at match time.
constexpr std::size_t kBambuMaterialAliasesLength = 128;  ///< Buffer size for BambuMaterialMappingEntry::aliases ('|'-separated tokens).
constexpr char kBambuMaterialAliasSeparator = '|';        ///< Separator character used inside BambuMaterialMappingEntry::aliases.

/// @brief One material's Bambu AMS profile, as loaded from a
///        bambu_materials.json "materials[]" entry: generic filament-setting
///        id, canonical wire tray_type text, Bambu's own default nozzle
///        temperature range for that material, and optional alternate
///        spellings. This is metadata for the AMS slot only -- it does not
///        affect the actual print temperature, which always comes from the
///        slicer/filament profile used for a print job, not from AMS slot
///        metadata.
struct BambuMaterialMappingEntry {
  char material[kBambuMaterialFieldLength]{};      ///< Canonical Spoolman material key this entry matches (compared case- and separator-insensitively, see services::resolveBambuMaterial()).
  char trayInfoIdx[kBambuTrayInfoIdxLength]{};      ///< Bambu generic filament profile id, sent as "tray_info_idx".
  char trayType[kBambuMaterialFieldLength]{};       ///< Canonical material text sent as "tray_type"; not necessarily identical to #material.
  std::uint16_t nozzleTempMinC = 0;                 ///< Bambu's default minimum nozzle temperature for this material.
  std::uint16_t nozzleTempMaxC = 0;                 ///< Bambu's default maximum nozzle temperature for this material.
  char aliases[kBambuMaterialAliasesLength]{};      ///< '|'-separated list of additional accepted spellings (may be empty); see #kBambuMaterialAliasSeparator.
};

/// @brief Fixed-capacity collection of BambuMaterialMappingEntry values, the
///        in-memory mirror of /config/bambu_materials.json's "materials[]"
///        array. Populated exclusively by tasks::storageTask() (see
///        services::parseBambuMaterialCatalog()) and published to every
///        reader via rtos::RtosContext::bambuMaterialMappings (an atomic
///        pointer swap, not a queue message -- see docs/architecture.md).
struct BambuMaterialMappingTable {
  BambuMaterialMappingEntry entries[kMaxBambuMaterialMappings]{};  ///< Backing storage; only the first #entryCount entries are valid.
  std::uint16_t entryCount = 0;   ///< Number of valid entries in #entries.
  std::uint32_t schemaVersion = 0;  ///< "schema_version" field of the source JSON document.
};
static_assert(std::is_trivially_copyable<BambuMaterialMappingEntry>::value,
              "BambuMaterialMappingEntry must be trivially copyable");
static_assert(std::is_trivially_copyable<BambuMaterialMappingTable>::value,
              "BambuMaterialMappingTable must be trivially copyable");

}  // namespace models
}  // namespace filament_station
