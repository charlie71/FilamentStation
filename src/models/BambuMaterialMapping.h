/**
 * @file
 * @brief In-RAM Bambu AMS material-mapping rule table, loaded at runtime from
 *        /config/bambu_materials.json (schema v2, see
 *        services/BambuMaterialCatalog.h and docs/bambu-protocol.md).
 *        Replaces the earlier schema-v1 flat material->profile table with a
 *        priority-ordered set of rules matched against Spoolman's
 *        material/name/manufacturer fields, so Bambu-specific profiles
 *        (e.g. "PLA Silk", "PLA High Speed", "Bambu PLA Basic") can be
 *        distinguished from the generic material class alone.
 */
#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

// Sized for headroom over the rule count introduced with the schema-v2
// migration (2026-08-30: 33 baseline SpoolmanDB materials + the previously
// migrated v1 catalog + support/special-profile rules, ~75 rules) --
// parseBambuMaterialCatalog() rejects a file with more rules than this
// rather than silently truncating it.
constexpr std::size_t kMaxBambuMaterialRules = 96;  ///< Maximum number of rules loadable from bambu_materials.json.
constexpr std::size_t kBambuMaterialRuleIdLength = 40;      ///< Buffer size for BambuMaterialRule::id (e.g. "generic-pla-high-speed").
constexpr std::size_t kBambuMaterialFieldLength = 24;       ///< Buffer size for one match value or BambuMaterialRuleResult::trayType.
constexpr std::size_t kBambuTrayInfoIdxLength = 12;         ///< Buffer size for BambuMaterialRuleResult::trayInfoIdx (e.g. "GFL99").
constexpr std::size_t kBambuMaterialReasonLength = 80;      ///< Buffer size for BambuMaterialRuleResult::reason.
// Match values are stored as one '|'-separated string per category rather
// than a fixed 2D array: most rules have 1-3 values per category, so a 2D
// array would mostly be padding (same reasoning as the old aliases field
// this replaces) -- the resolver walks the tokens at match time.
constexpr std::size_t kBambuMatchValuesLength = 160;  ///< Buffer size for one BambuMaterialMatch category ('|'-separated values).
constexpr char kBambuMatchValueSeparator = '|';       ///< Separator character used inside BambuMaterialMatch's fields.

/// @brief What a rule's match block requires to consider a filament a hit.
///        Categories left empty impose no restriction; present categories
///        are combined with AND, while the '|'-separated values within one
///        category are combined with OR (see services::resolveBambuMaterialRule()).
struct BambuMaterialMatch {
  char materialExact[kBambuMatchValuesLength]{};       ///< '|'-separated exact (post-normalization) matches against the input's material.
  char nameContainsAny[kBambuMatchValuesLength]{};     ///< '|'-separated substrings, any of which must appear (post-normalization) in the input's name.
  char manufacturerExact[kBambuMatchValuesLength]{};   ///< '|'-separated exact (post-normalization) matches against the input's manufacturer.
};

/// @brief Whether a rule resolves to a usable Bambu AMS profile or
///        deliberately declines to guess one.
enum class BambuMaterialRuleResultStatus : std::uint8_t {
  Mapped,        ///< trayInfoIdx/trayType/nozzleTempMinC/nozzleTempMaxC are valid.
  Unsupported,   ///< No Bambu profile exists for this material; #reason explains why.
};

/// @brief What a matching rule resolves to.
struct BambuMaterialRuleResult {
  BambuMaterialRuleResultStatus status = BambuMaterialRuleResultStatus::Unsupported;  ///< Mapped or Unsupported.
  char trayInfoIdx[kBambuTrayInfoIdxLength]{};   ///< Bambu generic filament profile id, valid if #status is Mapped.
  char trayType[kBambuMaterialFieldLength]{};    ///< Canonical material text sent as "tray_type", valid if #status is Mapped.
  std::uint16_t nozzleTempMinC = 0;              ///< Minimum nozzle temperature, valid if #status is Mapped.
  std::uint16_t nozzleTempMaxC = 0;              ///< Maximum nozzle temperature, valid if #status is Mapped.
  char reason[kBambuMaterialReasonLength]{};     ///< Human-readable explanation, valid (non-empty) if #status is Unsupported.
};

/// @brief One priority-ordered rule: matches a Spoolman
///        material/name/manufacturer combination against #match, and
///        resolves to #result if it wins (see
///        services::resolveBambuMaterialRule()).
struct BambuMaterialRule {
  char id[kBambuMaterialRuleIdLength]{};  ///< Unique, human-readable rule identifier (e.g. "generic-pla-silk"), for logging/diagnostics.
  std::int32_t priority = 0;              ///< Higher wins; multiple matching rules tied at the highest priority make resolution Ambiguous.
  BambuMaterialMatch match;               ///< Conditions a filament must meet for this rule to apply.
  BambuMaterialRuleResult result;         ///< What this rule resolves to if it wins.
};

/// @brief Fixed-capacity collection of BambuMaterialRule values, the
///        in-memory mirror of /config/bambu_materials.json's "rules[]"
///        array. Populated exclusively by tasks::storageTask() (see
///        services::parseBambuMaterialCatalog()) and published to every
///        reader via rtos::RtosContext::bambuMaterialMappings (an atomic
///        pointer swap, not a queue message -- see docs/architecture.md).
struct BambuMaterialRuleTable {
  BambuMaterialRule rules[kMaxBambuMaterialRules]{};  ///< Backing storage; only the first #ruleCount entries are valid.
  std::uint16_t ruleCount = 0;      ///< Number of valid entries in #rules.
  std::uint32_t schemaVersion = 0;  ///< "schema_version" field of the source JSON document (must be 2).
};
static_assert(std::is_trivially_copyable<BambuMaterialRule>::value,
              "BambuMaterialRule must be trivially copyable");
static_assert(std::is_trivially_copyable<BambuMaterialRuleTable>::value,
              "BambuMaterialRuleTable must be trivially copyable");

}  // namespace models
}  // namespace filament_station
