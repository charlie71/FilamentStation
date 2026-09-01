/**
 * @file
 * @brief Pure parser/validator and priority-rule resolver for the
 *        bambu_materials.json schema-v2 document format (see
 *        docs/bambu-protocol.md). No network or storage access here
 *        (AGENTS.md coding rules) -- tasks::storageTask() owns the actual
 *        file I/O and calls into this file to turn a parsed JsonDocument
 *        into a models::BambuMaterialRuleTable; tasks::bambuTask() calls
 *        into this file to resolve a Spoolman filament against that table.
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
  InvalidJson,              ///< Root is not a JSON object.
  MissingSchemaVersion,     ///< "schema_version" is absent or not an integer.
  UnsupportedSchemaVersion,  ///< "schema_version" is present but not 2, the only version this firmware understands.
  MissingRulesArray,        ///< "rules" is absent or not an array.
  TooManyEntries,           ///< "rules" has more entries than models::kMaxBambuMaterialRules.
  MissingRuleId,            ///< A rule's "id" is absent, empty, or exceeds its buffer.
  DuplicateRuleId,          ///< Two rules share the same "id".
  MissingPriority,          ///< A rule's "priority" is absent or not an integer.
  MissingMatch,             ///< A rule's "match" is absent or not an object.
  EmptyMatch,               ///< A rule's "match" object has no non-empty match category.
  InvalidMatchFieldType,    ///< A match category is present but not an array of non-empty strings, or a value is too long/exceeds the combined buffer.
  MissingStatus,            ///< A rule's "result.status" is absent.
  InvalidStatus,            ///< "result.status" is present but neither "mapped" nor "unsupported".
  MissingRequiredField,     ///< A "mapped" result is missing/empty tray_info_idx/tray_type, or a field exceeds its buffer.
  InvalidTemperatureRange,  ///< A "mapped" result's nozzle_temp_min/nozzle_temp_max are not both integers in (0, 400], or min > max.
  MissingReason,            ///< An "unsupported" result is missing/empty "reason".
};

/// @brief Result of parseBambuMaterialCatalog(): the error (Ok on success)
///        plus, for most error cases, the offending rule id/field for logging.
struct BambuMaterialCatalogResult {
  BambuMaterialCatalogError error = BambuMaterialCatalogError::Ok;  ///< Ok on success.
  char offendingKey[models::kBambuMaterialRuleIdLength]{};  ///< Rule id/material text implicated in the error, for FS_LOGW; empty if not applicable.
};

/// @brief Parses and fully validates a bambu_materials.json schema-v2
///        document (see docs/bambu-protocol.md for the schema) into `out`.
/// @param document Already-deserialized JSON document.
/// @param out Table to fill; left in an unspecified (partially-written)
///        state on failure -- callers must check the returned error before
///        using it, never fall back to a half-populated table.
/// @return BambuMaterialCatalogError::Ok on success, otherwise the specific
///         rejection reason. The whole document is rejected atomically --
///         never a mix of valid and invalid rules (docs/bambu-protocol.md).
BambuMaterialCatalogResult parseBambuMaterialCatalog(
    const JsonDocument& document, models::BambuMaterialRuleTable& out);

/// @brief Stable, lowercase machine-readable name for a
///        BambuMaterialCatalogError, for the "reason=" field of
///        "[BAMBU] Material mapping load failed" log lines.
/// @param error Error to name.
/// @return Static, NUL-terminated name.
const char* bambuMaterialCatalogErrorName(BambuMaterialCatalogError error);

/// @brief Free-text Spoolman fields a filament is resolved from. Carries no
///        MQTT/slot/AMS information -- purely the inputs the match rules
///        compare against (see models::BambuMaterialMatch).
struct BambuMaterialResolveInput {
  const char* material = "";      ///< Spoolman filament.material (e.g. "PLA", "PETG", "PLA-CF").
  const char* name = "";          ///< Spoolman filament.name / spool product name (e.g. "Silk PLA", "PLA Basic").
  const char* manufacturer = "";  ///< Spoolman filament vendor name (e.g. "Bambu Lab", "eSUN").
};

/// @brief How resolveBambuMaterialRule() classifies a resolution attempt.
enum class BambuMaterialResolveStatus : std::uint8_t {
  Mapped,        ///< A single highest-priority rule matched and resolves to a usable Bambu profile.
  Unsupported,   ///< A single highest-priority rule matched, but it deliberately declines to map this material.
  NoMatch,       ///< No rule matched at all.
  Ambiguous,     ///< More than one rule matched at the same highest priority -- a configuration error, never resolved by JSON order.
};

/// @brief Result of resolveBambuMaterialRule().
struct BambuMaterialResolveResult {
  BambuMaterialResolveStatus status = BambuMaterialResolveStatus::NoMatch;  ///< Outcome classification.
  const models::BambuMaterialRule* rule = nullptr;  ///< The winning rule, valid (non-null) only for Mapped/Unsupported. Points into `table` (see resolveBambuMaterialRule()'s parameter) -- same lifetime constraint as the old resolveBambuMaterial().
  std::int32_t matchedPriority = 0;  ///< Priority level at which the match (or tie) was found; 0 if NoMatch.
  char ambiguousRuleIds[96]{};       ///< Comma-separated ids of every rule tied at #matchedPriority, valid (non-empty) only for Ambiguous.
};

/// @brief Resolves a Spoolman filament to the single best-matching Bambu AMS
///        profile rule, by priority.
/// @param table Mapping table loaded from /config/bambu_materials.json (see
///        services::BambuMaterialCatalog, tasks::storageTask()).
/// @param input Free-text Spoolman fields to match against.
/// @return See services::BambuMaterialResolveResult. Never guesses/invents a
///         profile: exactly one rule must win at the highest matching
///         priority for Mapped/Unsupported to be returned.
BambuMaterialResolveResult resolveBambuMaterialRule(
    const models::BambuMaterialRuleTable& table,
    const BambuMaterialResolveInput& input);

}  // namespace services
}  // namespace filament_station
