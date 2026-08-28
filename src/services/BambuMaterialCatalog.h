/**
 * @file
 * @brief Pure parser/validator for the bambu_materials.json document format
 *        (see docs/bambu-protocol.md). No network or storage access here
 *        (AGENTS.md coding rules) -- tasks::storageTask() owns the actual
 *        file I/O and calls into this file to turn a parsed JsonDocument
 *        into a models::BambuMaterialMappingTable.
 */
#pragma once

#include <ArduinoJson.h>
#include <cstdint>

#include "models/BambuMaterialMapping.h"

namespace filament_station {
namespace services {

/// @brief Every way parseBambuMaterialCatalog() can reject a document.
enum class BambuMaterialCatalogError : std::uint8_t {
  Ok,
  InvalidJson,             ///< Root is not a JSON object.
  MissingSchemaVersion,    ///< "schema_version" is absent or not an integer.
  UnsupportedSchemaVersion,  ///< "schema_version" is present but not a version this firmware understands.
  MissingMaterialsArray,   ///< "materials" is absent or not an array.
  TooManyEntries,          ///< "materials" has more entries than models::kMaxBambuMaterialMappings.
  MissingRequiredField,    ///< An entry is missing/empty material/tray_info_idx/tray_type, or a field exceeds its buffer.
  InvalidTemperatureRange,  ///< nozzle_temp_min/nozzle_temp_max are not both integers in (0, 400], or min > max.
  InvalidAliasType,        ///< "aliases" is present but not an array of non-empty strings, or an alias is too long/exceeds the combined buffer.
  DuplicateLookupKey,      ///< Two entries (or an entry and an alias) normalize to the same lookup key.
};

/// @brief Result of parseBambuMaterialCatalog(): the error (Ok on success)
///        plus, for MissingRequiredField/DuplicateLookupKey, the offending
///        key/field for logging.
struct BambuMaterialCatalogResult {
  BambuMaterialCatalogError error = BambuMaterialCatalogError::Ok;  ///< Ok on success.
  char offendingKey[models::kBambuMaterialFieldLength]{};  ///< Material/alias text implicated in the error, for FS_LOGW; empty if not applicable.
};

/// @brief Parses and fully validates a bambu_materials.json document (see
///        docs/bambu-protocol.md for the schema) into `out`.
/// @param document Already-deserialized JSON document.
/// @param out Table to fill; left in an unspecified (partially-written)
///        state on failure -- callers must check the returned error before
///        using it, never fall back to a half-populated table.
/// @return BambuMaterialCatalogError::Ok on success, otherwise the specific
///         rejection reason. The whole document is rejected atomically --
///         never a mix of valid and invalid entries (docs/bambu-protocol.md).
BambuMaterialCatalogResult parseBambuMaterialCatalog(
    const JsonDocument& document, models::BambuMaterialMappingTable& out);

/// @brief Stable, lowercase machine-readable name for a
///        BambuMaterialCatalogError, for the "reason=" field of
///        "[BAMBU] Material mapping load failed" log lines.
/// @param error Error to name.
/// @return Static, NUL-terminated name.
const char* bambuMaterialCatalogErrorName(BambuMaterialCatalogError error);

}  // namespace services
}  // namespace filament_station
