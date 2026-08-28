/**
 * @file
 * @brief Implements services::parseBambuMaterialCatalog(): pure JSON
 *        parsing/validation for bambu_materials.json (see
 *        docs/bambu-protocol.md).
 */
#include "services/BambuMaterialCatalog.h"

#include <cstdio>
#include <cstring>

#include "services/BambuProtocol.h"

namespace filament_station {
namespace services {
namespace {

constexpr std::uint32_t kSupportedSchemaVersion = 1;
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

/// @brief Whether `key` normalizes (services::sameMaterialKey()) to the same
///        value as any key already parsed into `table`'s first `entryCount`
///        entries (their material name, or one of their aliases).
bool duplicatesExistingKey(const models::BambuMaterialMappingTable& table,
                           const char* key) {
  for (std::uint16_t index = 0; index < table.entryCount; ++index) {
    const models::BambuMaterialMappingEntry& entry = table.entries[index];
    if (sameMaterialKey(key, entry.material)) return true;
    const char* cursor = entry.aliases;
    while (*cursor != '\0') {
      char token[models::kBambuMaterialFieldLength]{};
      std::size_t length = 0;
      while (*cursor != '\0' &&
            *cursor != models::kBambuMaterialAliasSeparator &&
            length < sizeof(token) - 1U) {
        token[length++] = *cursor++;
      }
      token[length] = '\0';
      while (*cursor != '\0' && *cursor != models::kBambuMaterialAliasSeparator)
        ++cursor;
      if (*cursor == models::kBambuMaterialAliasSeparator) ++cursor;
      if (length > 0 && sameMaterialKey(key, token)) return true;
    }
  }
  return false;
}

}  // namespace

BambuMaterialCatalogResult parseBambuMaterialCatalog(
    const JsonDocument& document, models::BambuMaterialMappingTable& out) {
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

  if (!root["materials"].is<JsonArrayConst>()) {
    return makeError(BambuMaterialCatalogError::MissingMaterialsArray);
  }
  const JsonArrayConst materials = root["materials"].as<JsonArrayConst>();
  if (materials.size() > models::kMaxBambuMaterialMappings) {
    return makeError(BambuMaterialCatalogError::TooManyEntries);
  }

  // Writes directly into the caller-owned `out` rather than building a
  // second ~18 KiB models::BambuMaterialMappingTable on this task's stack
  // (StorageTask's stack is far too small for that) -- `out` is left
  // partially populated on any early-return failure below, which is safe
  // per this function's contract (see services/BambuMaterialCatalog.h):
  // callers only ever publish `out` after checking for BambuMaterialCatalogError::Ok.
  out.entryCount = 0;
  out.schemaVersion = schemaVersion;

  for (JsonObjectConst entryJson : materials) {
    if (!isNonEmptyString(entryJson["material"]) ||
        !isNonEmptyString(entryJson["tray_info_idx"]) ||
        !isNonEmptyString(entryJson["tray_type"])) {
      return makeError(BambuMaterialCatalogError::MissingRequiredField);
    }
    const char* materialText = entryJson["material"].as<const char*>();
    const char* trayInfoIdxText = entryJson["tray_info_idx"].as<const char*>();
    const char* trayTypeText = entryJson["tray_type"].as<const char*>();
    if (!fitsInBuffer(materialText, models::kBambuMaterialFieldLength) ||
        !fitsInBuffer(trayTypeText, models::kBambuMaterialFieldLength) ||
        !fitsInBuffer(trayInfoIdxText, models::kBambuTrayInfoIdxLength)) {
      return makeError(BambuMaterialCatalogError::MissingRequiredField,
                       materialText);
    }

    if (!entryJson["nozzle_temp_min"].is<std::uint16_t>() ||
        !entryJson["nozzle_temp_max"].is<std::uint16_t>()) {
      return makeError(BambuMaterialCatalogError::InvalidTemperatureRange,
                       materialText);
    }
    const auto tempMin = entryJson["nozzle_temp_min"].as<std::uint16_t>();
    const auto tempMax = entryJson["nozzle_temp_max"].as<std::uint16_t>();
    if (tempMin < kMinNozzleTempC || tempMin > kMaxNozzleTempC ||
        tempMax < kMinNozzleTempC || tempMax > kMaxNozzleTempC ||
        tempMin > tempMax) {
      return makeError(BambuMaterialCatalogError::InvalidTemperatureRange,
                       materialText);
    }

    if (duplicatesExistingKey(out, materialText)) {
      return makeError(BambuMaterialCatalogError::DuplicateLookupKey,
                       materialText);
    }

    models::BambuMaterialMappingEntry entry{};
    std::snprintf(entry.material, sizeof(entry.material), "%s", materialText);
    std::snprintf(entry.trayInfoIdx, sizeof(entry.trayInfoIdx), "%s",
                 trayInfoIdxText);
    std::snprintf(entry.trayType, sizeof(entry.trayType), "%s", trayTypeText);
    entry.nozzleTempMinC = tempMin;
    entry.nozzleTempMaxC = tempMax;

    // "aliases" is optional; if present it must be an array of non-empty
    // strings (docs/bambu-protocol.md) -- joined into one '|'-separated
    // buffer, see models::BambuMaterialMappingEntry::aliases.
    if (!entryJson["aliases"].isNull()) {
      if (!entryJson["aliases"].is<JsonArrayConst>()) {
        return makeError(BambuMaterialCatalogError::InvalidAliasType,
                         materialText);
      }
      std::size_t aliasesLength = 0;
      for (JsonVariantConst aliasJson : entryJson["aliases"].as<JsonArrayConst>()) {
        if (!isNonEmptyString(aliasJson)) {
          return makeError(BambuMaterialCatalogError::InvalidAliasType,
                           materialText);
        }
        const char* aliasText = aliasJson.as<const char*>();
        if (!fitsInBuffer(aliasText, models::kBambuMaterialFieldLength)) {
          return makeError(BambuMaterialCatalogError::InvalidAliasType,
                           aliasText);
        }
        // Only checked against *other, already-committed* entries here
        // (the entry currently being built is not yet in `out`) -- an
        // alias that merely restates its own material under a different
        // separator style (e.g. "PLA CF" as an alias of material
        // "PLA-CF", both already equal under sameMaterialKey()) is
        // harmless, explicitly allowed by docs/bambu-protocol.md's schema
        // examples, and not an ambiguity: it still resolves to this same entry.
        if (duplicatesExistingKey(out, aliasText)) {
          return makeError(BambuMaterialCatalogError::DuplicateLookupKey,
                           aliasText);
        }
        const std::size_t aliasTextLength = std::strlen(aliasText);
        // +1 for the separator byte written before every alias after the
        // first; the trailing NUL is implicit (entry.aliases is
        // zero-initialized and never fully filled here, see the bounds
        // check below).
        const std::size_t needed =
            aliasesLength + (aliasesLength > 0 ? 1U : 0U) + aliasTextLength;
        if (needed >= models::kBambuMaterialAliasesLength) {
          return makeError(BambuMaterialCatalogError::InvalidAliasType,
                           aliasText);
        }
        if (aliasesLength > 0) {
          entry.aliases[aliasesLength++] = models::kBambuMaterialAliasSeparator;
        }
        std::memcpy(entry.aliases + aliasesLength, aliasText, aliasTextLength);
        aliasesLength += aliasTextLength;
        entry.aliases[aliasesLength] = '\0';
      }
    }

    out.entries[out.entryCount++] = entry;
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
    case BambuMaterialCatalogError::MissingMaterialsArray:
      return "missing_materials_array";
    case BambuMaterialCatalogError::TooManyEntries:
      return "too_many_entries";
    case BambuMaterialCatalogError::MissingRequiredField:
      return "missing_required_field";
    case BambuMaterialCatalogError::InvalidTemperatureRange:
      return "invalid_temperature_range";
    case BambuMaterialCatalogError::InvalidAliasType:
      return "invalid_alias_type";
    case BambuMaterialCatalogError::DuplicateLookupKey:
      return "duplicate_lookup_key";
  }
  return "unknown";
}

}  // namespace services
}  // namespace filament_station
