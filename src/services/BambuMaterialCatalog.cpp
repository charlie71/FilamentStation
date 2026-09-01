/**
 * @file
 * @brief Implements services::parseBambuMaterialCatalog() (schema-v2 JSON
 *        parsing/validation) and services::resolveBambuMaterialRule() (the
 *        priority-ordered rule resolver), for bambu_materials.json (see
 *        docs/bambu-protocol.md).
 */
#include "services/BambuMaterialCatalog.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace filament_station {
namespace services {
namespace {

constexpr std::uint32_t kSupportedSchemaVersion = 2;
constexpr std::uint16_t kMinNozzleTempC = 1;    ///< Exclusive lower bound (temperatures must be > 0).
constexpr std::uint16_t kMaxNozzleTempC = 400;  ///< Inclusive upper bound.

/// @brief Sets `result.error`/`offendingKey` in one call.
BambuMaterialCatalogResult makeError(BambuMaterialCatalogError error,
                                     const char* offendingKey = "") {
  BambuMaterialCatalogResult result{};
  result.error = error;
  std::snprintf(result.offendingKey, sizeof(result.offendingKey), "%s",
               offendingKey);
  return result;
}

/// @brief Whether a JSON field is a non-empty string.
bool isNonEmptyString(JsonVariantConst value) {
  return value.is<const char*>() && value.as<const char*>()[0] != '\0';
}

/// @brief Whether `text` fits (including the NUL terminator) into a buffer
///        of `capacity` bytes.
bool fitsInBuffer(const char* text, std::size_t capacity) {
  return std::strlen(text) < capacity;
}

/// @brief Whether `id` matches (case-sensitively, exact) the id of any of
///        `table`'s first `ruleCount` rules.
bool duplicateRuleId(const models::BambuMaterialRuleTable& table,
                     const char* id) {
  for (std::uint16_t index = 0; index < table.ruleCount; ++index) {
    if (std::strcmp(table.rules[index].id, id) == 0) return true;
  }
  return false;
}

/// @brief Parses one match category ("material_exact"/"name_contains_any"/
///        "manufacturer_exact") into `out`'s '|'-separated buffer.
/// @param categoryJson The category's JSON value; null/absent is valid and
///        yields an empty `out` (no restriction from this category).
/// @param out Destination buffer (models::BambuMaterialMatch field).
/// @param outCapacity Size of `out` in bytes.
/// @return false if `categoryJson` is present but not an array of
///        non-empty strings, an entry exceeds models::kBambuMaterialFieldLength,
///        or the joined values would exceed `outCapacity`.
bool parseMatchCategory(JsonVariantConst categoryJson, char* out,
                        std::size_t outCapacity) {
  out[0] = '\0';
  if (categoryJson.isNull()) return true;
  if (!categoryJson.is<JsonArrayConst>()) return false;
  std::size_t length = 0;
  for (JsonVariantConst valueJson : categoryJson.as<JsonArrayConst>()) {
    if (!isNonEmptyString(valueJson)) return false;
    const char* text = valueJson.as<const char*>();
    if (!fitsInBuffer(text, models::kBambuMaterialFieldLength)) return false;
    const std::size_t textLength = std::strlen(text);
    const std::size_t needed =
        length + (length > 0 ? 1U : 0U) + textLength;
    if (needed >= outCapacity) return false;
    if (length > 0) out[length++] = models::kBambuMatchValueSeparator;
    std::memcpy(out + length, text, textLength);
    length += textLength;
    out[length] = '\0';
  }
  return true;
}

/// @brief Parses a rule's "match" object into `match`.
/// @return false if "match" is not an object, or any category fails
///        parseMatchCategory().
bool parseMatch(JsonVariantConst matchJson, models::BambuMaterialMatch& match) {
  if (!matchJson.is<JsonObjectConst>()) return false;
  const JsonObjectConst matchObj = matchJson.as<JsonObjectConst>();
  if (!parseMatchCategory(matchObj["material_exact"], match.materialExact,
                          sizeof(match.materialExact)))
    return false;
  if (!parseMatchCategory(matchObj["name_contains_any"], match.nameContainsAny,
                          sizeof(match.nameContainsAny)))
    return false;
  if (!parseMatchCategory(matchObj["manufacturer_exact"],
                          match.manufacturerExact,
                          sizeof(match.manufacturerExact)))
    return false;
  return true;
}

/// @brief Whether a parsed match object has at least one non-empty category.
bool matchHasAnyCondition(const models::BambuMaterialMatch& match) {
  return match.materialExact[0] != '\0' || match.nameContainsAny[0] != '\0' ||
        match.manufacturerExact[0] != '\0';
}

}  // namespace

BambuMaterialCatalogResult parseBambuMaterialCatalog(
    const JsonDocument& document, models::BambuMaterialRuleTable& out) {
  if (!document.is<JsonObjectConst>()) {
    return makeError(BambuMaterialCatalogError::InvalidJson);
  }
  const JsonObjectConst root = document.as<JsonObjectConst>();

  if (!root["schema_version"].is<std::uint32_t>()) {
    return makeError(BambuMaterialCatalogError::MissingSchemaVersion);
  }
  const auto schemaVersion = root["schema_version"].as<std::uint32_t>();
  if (schemaVersion != kSupportedSchemaVersion) {
    return makeError(BambuMaterialCatalogError::UnsupportedSchemaVersion);
  }

  if (!root["rules"].is<JsonArrayConst>()) {
    return makeError(BambuMaterialCatalogError::MissingRulesArray);
  }
  const JsonArrayConst rules = root["rules"].as<JsonArrayConst>();
  if (rules.size() > models::kMaxBambuMaterialRules) {
    return makeError(BambuMaterialCatalogError::TooManyEntries);
  }

  // Writes directly into the caller-owned `out` rather than building a
  // second large models::BambuMaterialRuleTable on this task's stack
  // (StorageTask's stack is far too small for that) -- `out` is left
  // partially populated on any early-return failure below, which is safe
  // per this function's contract (see services/BambuMaterialCatalog.h):
  // callers only ever publish `out` after checking for BambuMaterialCatalogError::Ok.
  out.ruleCount = 0;
  out.schemaVersion = schemaVersion;

  for (JsonObjectConst ruleJson : rules) {
    if (!isNonEmptyString(ruleJson["id"])) {
      return makeError(BambuMaterialCatalogError::MissingRuleId);
    }
    const char* idText = ruleJson["id"].as<const char*>();
    if (!fitsInBuffer(idText, models::kBambuMaterialRuleIdLength)) {
      return makeError(BambuMaterialCatalogError::MissingRuleId, idText);
    }
    if (duplicateRuleId(out, idText)) {
      return makeError(BambuMaterialCatalogError::DuplicateRuleId, idText);
    }

    if (!ruleJson["priority"].is<std::int32_t>()) {
      return makeError(BambuMaterialCatalogError::MissingPriority, idText);
    }
    const auto priority = ruleJson["priority"].as<std::int32_t>();

    if (!ruleJson["match"].is<JsonObjectConst>()) {
      return makeError(BambuMaterialCatalogError::MissingMatch, idText);
    }
    models::BambuMaterialMatch match{};
    if (!parseMatch(ruleJson["match"], match)) {
      return makeError(BambuMaterialCatalogError::InvalidMatchFieldType,
                       idText);
    }
    if (!matchHasAnyCondition(match)) {
      return makeError(BambuMaterialCatalogError::EmptyMatch, idText);
    }

    const JsonVariantConst resultJson = ruleJson["result"];
    if (!resultJson.is<JsonObjectConst>() ||
        !isNonEmptyString(resultJson["status"])) {
      return makeError(BambuMaterialCatalogError::MissingStatus, idText);
    }
    const char* statusText = resultJson["status"].as<const char*>();

    models::BambuMaterialRuleResult result{};
    if (std::strcmp(statusText, "mapped") == 0) {
      result.status = models::BambuMaterialRuleResultStatus::Mapped;
      if (!isNonEmptyString(resultJson["tray_info_idx"]) ||
          !isNonEmptyString(resultJson["tray_type"])) {
        return makeError(BambuMaterialCatalogError::MissingRequiredField,
                         idText);
      }
      const char* trayInfoIdxText =
          resultJson["tray_info_idx"].as<const char*>();
      const char* trayTypeText = resultJson["tray_type"].as<const char*>();
      if (!fitsInBuffer(trayInfoIdxText, models::kBambuTrayInfoIdxLength) ||
          !fitsInBuffer(trayTypeText, models::kBambuMaterialFieldLength)) {
        return makeError(BambuMaterialCatalogError::MissingRequiredField,
                         idText);
      }
      if (!resultJson["nozzle_temp_min"].is<std::uint16_t>() ||
          !resultJson["nozzle_temp_max"].is<std::uint16_t>()) {
        return makeError(BambuMaterialCatalogError::InvalidTemperatureRange,
                         idText);
      }
      const auto tempMin = resultJson["nozzle_temp_min"].as<std::uint16_t>();
      const auto tempMax = resultJson["nozzle_temp_max"].as<std::uint16_t>();
      if (tempMin < kMinNozzleTempC || tempMin > kMaxNozzleTempC ||
          tempMax < kMinNozzleTempC || tempMax > kMaxNozzleTempC ||
          tempMin > tempMax) {
        return makeError(BambuMaterialCatalogError::InvalidTemperatureRange,
                         idText);
      }
      std::snprintf(result.trayInfoIdx, sizeof(result.trayInfoIdx), "%s",
                   trayInfoIdxText);
      std::snprintf(result.trayType, sizeof(result.trayType), "%s",
                   trayTypeText);
      result.nozzleTempMinC = tempMin;
      result.nozzleTempMaxC = tempMax;
    } else if (std::strcmp(statusText, "unsupported") == 0) {
      result.status = models::BambuMaterialRuleResultStatus::Unsupported;
      if (!isNonEmptyString(resultJson["reason"])) {
        return makeError(BambuMaterialCatalogError::MissingReason, idText);
      }
      const char* reasonText = resultJson["reason"].as<const char*>();
      if (!fitsInBuffer(reasonText, models::kBambuMaterialReasonLength)) {
        return makeError(BambuMaterialCatalogError::MissingReason, idText);
      }
      std::snprintf(result.reason, sizeof(result.reason), "%s", reasonText);
    } else {
      return makeError(BambuMaterialCatalogError::InvalidStatus, idText);
    }

    models::BambuMaterialRule rule{};
    std::snprintf(rule.id, sizeof(rule.id), "%s", idText);
    rule.priority = priority;
    rule.match = match;
    rule.result = result;
    out.rules[out.ruleCount++] = rule;
  }

  return {};
}

const char* bambuMaterialCatalogErrorName(BambuMaterialCatalogError error) {
  switch (error) {
    case BambuMaterialCatalogError::Ok:
      return "ok";
    case BambuMaterialCatalogError::InvalidJson:
      return "invalid_json";
    case BambuMaterialCatalogError::MissingSchemaVersion:
      return "missing_schema_version";
    case BambuMaterialCatalogError::UnsupportedSchemaVersion:
      return "unsupported_schema_version";
    case BambuMaterialCatalogError::MissingRulesArray:
      return "missing_rules_array";
    case BambuMaterialCatalogError::TooManyEntries:
      return "too_many_entries";
    case BambuMaterialCatalogError::MissingRuleId:
      return "missing_rule_id";
    case BambuMaterialCatalogError::DuplicateRuleId:
      return "duplicate_rule_id";
    case BambuMaterialCatalogError::MissingPriority:
      return "missing_priority";
    case BambuMaterialCatalogError::MissingMatch:
      return "missing_match";
    case BambuMaterialCatalogError::EmptyMatch:
      return "empty_match";
    case BambuMaterialCatalogError::InvalidMatchFieldType:
      return "invalid_match_field_type";
    case BambuMaterialCatalogError::MissingStatus:
      return "missing_status";
    case BambuMaterialCatalogError::InvalidStatus:
      return "invalid_status";
    case BambuMaterialCatalogError::MissingRequiredField:
      return "missing_required_field";
    case BambuMaterialCatalogError::InvalidTemperatureRange:
      return "invalid_temperature_range";
    case BambuMaterialCatalogError::MissingReason:
      return "missing_reason";
  }
  return "unknown";
}

namespace {

/// @brief Normalizes free text for rule matching: trims leading/trailing
///        whitespace, lowercases, and collapses internal whitespace runs to
///        a single space. Deliberately does NOT strip '-'/'_'/'+' or other
///        punctuation -- "PLA", "PLA-CF" and "PLA+" must stay distinguishable
///        (Nutzerwunsch 2026-08-30, unlike the old, more aggressive
///        sameMaterialKey() this replaces).
/// @param input Raw text (may be nullptr, treated as empty).
/// @param out Destination buffer.
/// @param outCapacity Size of `out` in bytes.
void normalizeMatchText(const char* input, char* out,
                        std::size_t outCapacity) {
  if (outCapacity == 0) return;
  if (input == nullptr) input = "";
  std::size_t written = 0;
  bool pendingSpace = false;
  bool started = false;
  for (const char* cursor = input; *cursor != '\0'; ++cursor) {
    const unsigned char c = static_cast<unsigned char>(*cursor);
    if (std::isspace(c)) {
      if (started) pendingSpace = true;
      continue;
    }
    if (pendingSpace) {
      if (written + 1 >= outCapacity) break;
      out[written++] = ' ';
      pendingSpace = false;
    }
    if (written + 1 >= outCapacity) break;
    out[written++] = static_cast<char>(std::tolower(c));
    started = true;
  }
  out[written] = '\0';
}

/// @brief Whether any '|'-separated value in `categoryValues` matches
///        `normalizedInput`, either exactly or as a substring.
/// @param categoryValues Raw (non-normalized) '|'-separated values from a
///        models::BambuMaterialMatch field; empty means "no restriction"
///        (always matches).
/// @param exact true for an exact (post-normalization) match, false for a
///        substring/"contains" match.
/// @param normalizedInput Already-normalizeMatchText()-ed input to compare against.
bool matchCategory(const char* categoryValues, bool exact,
                   const char* normalizedInput) {
  if (categoryValues[0] == '\0') return true;
  const char* cursor = categoryValues;
  while (*cursor != '\0') {
    char rawToken[models::kBambuMatchValuesLength]{};
    std::size_t length = 0;
    while (*cursor != '\0' &&
          *cursor != models::kBambuMatchValueSeparator &&
          length < sizeof(rawToken) - 1U) {
      rawToken[length++] = *cursor++;
    }
    rawToken[length] = '\0';
    while (*cursor != '\0' && *cursor != models::kBambuMatchValueSeparator)
      ++cursor;
    if (*cursor == models::kBambuMatchValueSeparator) ++cursor;
    if (length == 0) continue;

    char normalizedToken[models::kBambuMatchValuesLength]{};
    normalizeMatchText(rawToken, normalizedToken, sizeof(normalizedToken));
    if (exact) {
      if (std::strcmp(normalizedToken, normalizedInput) == 0) return true;
    } else if (normalizedToken[0] != '\0' &&
              std::strstr(normalizedInput, normalizedToken) != nullptr) {
      return true;
    }
  }
  return false;
}

/// @brief Whether `rule` matches the given already-normalized inputs (see
///        services::resolveBambuMaterialRule()'s AND/OR semantics).
bool ruleMatches(const models::BambuMaterialRule& rule,
                 const char* normalizedMaterial, const char* normalizedName,
                 const char* normalizedManufacturer) {
  return matchCategory(rule.match.materialExact, true, normalizedMaterial) &&
        matchCategory(rule.match.nameContainsAny, false, normalizedName) &&
        matchCategory(rule.match.manufacturerExact, true,
                      normalizedManufacturer);
}

}  // namespace

BambuMaterialResolveResult resolveBambuMaterialRule(
    const models::BambuMaterialRuleTable& table,
    const BambuMaterialResolveInput& input) {
  BambuMaterialResolveResult result{};

  char normalizedMaterial[models::kBambuMatchValuesLength]{};
  char normalizedName[models::kBambuMatchValuesLength]{};
  char normalizedManufacturer[models::kBambuMatchValuesLength]{};
  normalizeMatchText(input.material, normalizedMaterial,
                     sizeof(normalizedMaterial));
  normalizeMatchText(input.name, normalizedName, sizeof(normalizedName));
  normalizeMatchText(input.manufacturer, normalizedManufacturer,
                     sizeof(normalizedManufacturer));

  // Pass 1: highest priority among all matching rules. Not decided by JSON
  // order (Nutzerwunsch 2026-08-30) -- a second pass below collects every
  // rule tied at that priority to detect/report ambiguity.
  std::int32_t bestPriority = 0;
  bool haveMatch = false;
  for (std::uint16_t index = 0; index < table.ruleCount; ++index) {
    const models::BambuMaterialRule& rule = table.rules[index];
    if (!ruleMatches(rule, normalizedMaterial, normalizedName,
                     normalizedManufacturer))
      continue;
    if (!haveMatch || rule.priority > bestPriority) {
      bestPriority = rule.priority;
      haveMatch = true;
    }
  }
  if (!haveMatch) {
    result.status = BambuMaterialResolveStatus::NoMatch;
    return result;
  }
  result.matchedPriority = bestPriority;

  const models::BambuMaterialRule* winner = nullptr;
  std::uint16_t tieCount = 0;
  std::size_t idsLength = 0;
  for (std::uint16_t index = 0; index < table.ruleCount; ++index) {
    const models::BambuMaterialRule& rule = table.rules[index];
    if (rule.priority != bestPriority ||
        !ruleMatches(rule, normalizedMaterial, normalizedName,
                     normalizedManufacturer))
      continue;
    ++tieCount;
    if (winner == nullptr) winner = &rule;
    const std::size_t idLength = std::strlen(rule.id);
    const std::size_t needed =
        idsLength + (idsLength > 0 ? 2U : 0U) + idLength;
    if (needed < sizeof(result.ambiguousRuleIds)) {
      if (idsLength > 0) {
        result.ambiguousRuleIds[idsLength++] = ',';
      }
      std::memcpy(result.ambiguousRuleIds + idsLength, rule.id, idLength);
      idsLength += idLength;
      result.ambiguousRuleIds[idsLength] = '\0';
    }
  }

  if (tieCount > 1) {
    result.status = BambuMaterialResolveStatus::Ambiguous;
    result.rule = nullptr;
    return result;
  }

  result.rule = winner;
  result.status = winner->result.status ==
                          models::BambuMaterialRuleResultStatus::Mapped
                      ? BambuMaterialResolveStatus::Mapped
                      : BambuMaterialResolveStatus::Unsupported;
  return result;
}

}  // namespace services
}  // namespace filament_station
