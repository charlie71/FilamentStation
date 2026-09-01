/**
 * @file
 * @brief Implements the services::validateVendor()/validateFilament()/
 *        validateWeightUpdate()/mapTagDefinition() catalog helpers.
 */
#include "services/SpoolmanCatalog.h"

#include <cctype>
#include <cstring>
#include <cstdio>

namespace filament_station {
namespace services {
namespace {

/// @brief Advances past leading whitespace.
/// @param value NUL-terminated string.
/// @return Pointer to the first non-whitespace character, or the terminating NUL.
const char* skipSpaces(const char* value) {
  while (*value != '\0' && std::isspace(static_cast<unsigned char>(*value)))
    ++value;
  return value;
}

/// @brief Whether a string is empty or contains only whitespace.
/// @param value NUL-terminated string.
/// @return true if blank.
bool isBlank(const char* value) {
  value = skipSpaces(value);
  return *value == '\0';
}

/// @brief Case-insensitive comparison, ignoring leading/trailing whitespace.
/// @param left First string.
/// @param right Second string.
/// @return true if they are equal once trimmed and case-folded.
bool equalNormalized(const char* left, const char* right) {
  left = skipSpaces(left);
  right = skipSpaces(right);
  std::size_t leftLength = std::strlen(left);
  std::size_t rightLength = std::strlen(right);
  while (leftLength > 0 &&
         std::isspace(static_cast<unsigned char>(left[leftLength - 1])))
    --leftLength;
  while (rightLength > 0 &&
         std::isspace(static_cast<unsigned char>(right[rightLength - 1])))
    --rightLength;
  if (leftLength != rightLength) return false;
  for (std::size_t index = 0; index < leftLength; ++index) {
    if (std::tolower(static_cast<unsigned char>(left[index])) !=
        std::tolower(static_cast<unsigned char>(right[index])))
      return false;
  }
  return true;
}

/// @brief Whether a color code is empty, or a valid 6/8-digit hex string
///        with an optional leading '#'.
/// @param color NUL-terminated color string.
/// @return true if valid or empty.
bool validColor(const char* color) {
  if (color[0] == '\0') return true;
  if (color[0] == '#') ++color;
  const std::size_t length = std::strlen(color);
  if (length != 6 && length != 8) return false;
  for (std::size_t index = 0; index < length; ++index) {
    if (!std::isxdigit(static_cast<unsigned char>(color[index]))) return false;
  }
  return true;
}

/// @brief Compares two color codes ignoring a leading '#' and formatting.
/// @param left First color string.
/// @param right Second color string.
/// @return true if they represent the same color.
bool equalColor(const char* left, const char* right) {
  while (*left == '#') ++left;
  while (*right == '#') ++right;
  return equalNormalized(left, right);
}

}  // namespace

CatalogValidationError validateVendor(const models::SpoolmanVendor& vendor) {
  if (isBlank(vendor.name)) return CatalogValidationError::MissingName;
  if (vendor.emptySpoolWeightGrams < 0.0F)
    return CatalogValidationError::InvalidSpoolWeight;
  return CatalogValidationError::None;
}

CatalogValidationError validateFilament(
    const models::SpoolmanFilament& filament) {
  if (filament.vendorId == 0) return CatalogValidationError::MissingVendor;
  if (isBlank(filament.name)) return CatalogValidationError::MissingName;
  if (isBlank(filament.material))
    return CatalogValidationError::MissingMaterial;
  if (filament.densityGramsPerCm3 <= 0.0F)
    return CatalogValidationError::InvalidDensity;
  if (filament.diameterMillimeters <= 0.0F)
    return CatalogValidationError::InvalidDiameter;
  if (filament.weightGrams < 0.0F)
    return CatalogValidationError::InvalidWeight;
  if (filament.emptySpoolWeightGrams < 0.0F)
    return CatalogValidationError::InvalidSpoolWeight;
  if (!validColor(filament.colorHex))
    return CatalogValidationError::InvalidColor;
  if (filament.nozzleTemperatureC < 0 || filament.bedTemperatureC < 0)
    return CatalogValidationError::InvalidTemperature;
  return CatalogValidationError::None;
}

const char* catalogValidationMessage(CatalogValidationError error) {
  switch (error) {
    case CatalogValidationError::None: return "";
    case CatalogValidationError::MissingName: return "Name fehlt";
    case CatalogValidationError::MissingMaterial: return "Material fehlt";
    case CatalogValidationError::MissingVendor: return "Hersteller fehlt";
    case CatalogValidationError::InvalidDensity: return "Dichte muss gr\xC3\xB6\xC3\x9F" "er als null sein";
    case CatalogValidationError::InvalidDiameter: return "Durchmesser muss gr\xC3\xB6\xC3\x9F" "er als null sein";
    case CatalogValidationError::InvalidWeight: return "Filamentgewicht ist ung\xC3\xBCltig";
    case CatalogValidationError::InvalidSpoolWeight: return "Leergewicht ist ung\xC3\xBCltig";
    case CatalogValidationError::InvalidColor: return "Farbcode ist ung\xC3\xBCltig";
    case CatalogValidationError::InvalidTemperature: return "Temperatur ist ung\xC3\xBCltig";
  }
  return "Ung\xC3\xBCltige Katalogdaten";
}

bool sameVendor(const models::SpoolmanVendor& left,
                const models::SpoolmanVendor& right) {
  return equalNormalized(left.name, right.name);
}

bool sameFilament(const models::SpoolmanFilament& left,
                  const models::SpoolmanFilament& right) {
  return left.vendorId == right.vendorId &&
         equalNormalized(left.name, right.name) &&
         equalNormalized(left.material, right.material) &&
         equalColor(left.colorHex, right.colorHex);
}

WeightUpdateValidationError validateWeightUpdate(
    const models::SpoolmanWeightUpdate& update) {
  if (update.spoolId == 0)
    return WeightUpdateValidationError::MissingSpool;
  if (update.remainingWeightGrams < 0.0F)
    return WeightUpdateValidationError::InvalidRemainingWeight;
  if (update.updateInitialWeight && update.initialWeightGrams < 0.0F)
    return WeightUpdateValidationError::InvalidInitialWeight;
  if (update.updateEmptySpoolWeight && update.emptySpoolWeightGrams < 0.0F)
    return WeightUpdateValidationError::InvalidEmptySpoolWeight;
  return WeightUpdateValidationError::None;
}

namespace {
/// @brief Looks up a hardcoded typical density for a known filament material.
/// @param material Material name (case-insensitive, e.g. "PLA"), matched
///        verbatim (only leading/trailing whitespace is trimmed, no
///        separator/prefix normalization) -- so compound names like a Bambu
///        support-material tag's "Support for PLA" need their own explicit
///        entry, matching neither "Support" nor "PLA" alone.
/// @return Density in g/cm3, or 0.0F if the material is not recognized (used
///         to reject import of materials with no safe default density).
// Deliberately partial coverage, not a guess-based fallback (e.g. no
// "strip 'Support for '/'-CF' and reuse the base material's density"
// shortcut): a support/composite variant's real density can differ
// meaningfully from its base polymer (Nutzerbericht 2026-08-28, "Support
// for PLA" -- 1.33 g/cm3, notably denser than plain PLA's 1.24), so an
// unrecognized material is still rejected outright rather than approximated,
// same fail-closed philosophy as resolveBambuMaterialRule() (never invent/guess
// a value for something not explicitly verified). The composite/support
// entries below are sourced from Donkie/SpoolmanDB's Bambu Lab product data
// (github.com/Donkie/SpoolmanDB, community-maintained from Bambu's own
// published spec sheets) -- covers what that database lists a real Bambu
// product for; several material names this project's own
// data/bambu-materials/bambu_materials.json already recognizes (e.g. most
// other "Support For ..." variants, PPA-CF/GF, PCTG, PP-CF/GF, HIPS, PE,
// PHA, BVOH, EVA) have no verified density source yet and are intentionally
// left unrecognized here rather than guessed -- extend this table once a
// real source is found, see docs/bambu-protocol.md.
float materialDensity(const char* material) {
  /// @brief One (material name, density) lookup entry for materialDensity().
  struct Entry {
    const char* name;  ///< Material name, matched case-insensitively.
    float density;     ///< Density in g/cm3.
  };
  static const Entry entries[] = {
      {"PLA", 1.24F}, {"PETG", 1.27F}, {"ABS", 1.04F},
      {"ASA", 1.07F}, {"TPU", 1.21F}, {"PA", 1.14F},
      {"PA6", 1.14F}, {"PA11", 1.04F}, {"PA12", 1.01F},
      {"PC", 1.20F}, {"PVA", 1.23F}, {"HIPS", 1.04F},
      {"PP", 0.90F}, {"PCTG", 1.23F},
      // Bambu composite/support materials (Donkie/SpoolmanDB, see the doc
      // comment above) -- exact tag/catalog text, not the base material name.
      {"PLA-CF", 1.22F}, {"PETG-CF", 1.25F}, {"ABS-GF", 1.08F},
      {"ASA-CF", 1.02F}, {"ASA-AERO", 0.99F}, {"PA6-CF", 1.09F},
      {"PA6-GF", 1.09F}, {"PAHT-CF", 1.06F}, {"PET-CF", 1.29F},
      {"PPS-CF", 1.26F}, {"TPU 95A", 1.22F}, {"TPU-AMS", 1.26F},
      // Support materials -- matched against the exact wording Bambu prints
      // on the physical tag (also listed as aliases in
      // data/bambu-materials/bambu_materials.json); notably NOT the same
      // density as their named base material, see the doc comment above.
      {"Support for PLA", 1.33F}, {"Support for PLA/PETG", 1.28F},
      {"Support for ABS", 1.16F}, {"Support for PA/PET", 1.17F}};
  for (const Entry& entry : entries)
    if (equalNormalized(material, entry.name)) return entry.density;
  return 0.0F;
}

/// @brief Whether a tag format is one Spoolman import currently supports.
/// @param format Tag format to check.
/// @return true for BambuLab/OpenPrintTag/OpenTag3D/Legacy.
bool supportedImportFormat(models::TagFormat format) {
  return format == models::TagFormat::BambuLab ||
         format == models::TagFormat::OpenPrintTag ||
         format == models::TagFormat::OpenTag3D ||
         format == models::TagFormat::Legacy;
}
}  // namespace

TagImportValidationError mapTagDefinition(
    const models::TagDefinition& definition,
    models::SpoolmanImportDefinition& result) {
  result = {};
  if (!supportedImportFormat(definition.format))
    return TagImportValidationError::UnsupportedFormat;
  if (isBlank(definition.vendor)) return TagImportValidationError::MissingVendor;
  if (isBlank(definition.material)) return TagImportValidationError::MissingMaterial;
  if (isBlank(definition.filamentName))
    return TagImportValidationError::MissingFilament;
  if (isBlank(definition.colorCode)) return TagImportValidationError::MissingColor;
  if (!validColor(definition.colorCode))
    return TagImportValidationError::InvalidColor;
  if (definition.nominalFilamentWeightG <= 0.0F)
    return TagImportValidationError::MissingWeight;
  if (definition.emptySpoolWeightG < 0.0F)
    return TagImportValidationError::InvalidWeight;
  if (definition.nozzleTempMinC < 0 || definition.nozzleTempMaxC < 0 ||
      (definition.nozzleTempMaxC > 0 &&
       definition.nozzleTempMinC > definition.nozzleTempMaxC))
    return TagImportValidationError::InvalidTemperature;
  const float density = materialDensity(definition.material);
  if (density <= 0.0F) return TagImportValidationError::UnsupportedMaterial;

  std::snprintf(result.vendor.name, sizeof(result.vendor.name), "%s",
                definition.vendor);
  result.vendor.emptySpoolWeightGrams = definition.emptySpoolWeightG;
  std::snprintf(result.filament.name, sizeof(result.filament.name), "%s",
                definition.filamentName);
  std::snprintf(result.filament.material, sizeof(result.filament.material),
                "%s", definition.material);
  const char* color = definition.colorCode[0] == '#'
                          ? definition.colorCode + 1
                          : definition.colorCode;
  std::snprintf(result.filament.colorHex, sizeof(result.filament.colorHex),
                "%s", color);
  result.filament.densityGramsPerCm3 = density;
  // All four supported filament-tag formats describe FFF filament. Their
  // current V1 profiles target the standard 1.75 mm diameter.
  result.filament.diameterMillimeters = 1.75F;
  result.filament.weightGrams = definition.nominalFilamentWeightG;
  result.filament.emptySpoolWeightGrams = definition.emptySpoolWeightG;
  if (definition.nozzleTempMinC > 0 && definition.nozzleTempMaxC > 0)
    result.filament.nozzleTemperatureC = static_cast<std::int16_t>(
        (definition.nozzleTempMinC + definition.nozzleTempMaxC) / 2);
  else
    result.filament.nozzleTemperatureC = definition.nozzleTempMaxC > 0
                                             ? definition.nozzleTempMaxC
                                             : definition.nozzleTempMinC;
  result.initialWeightGrams = definition.nominalFilamentWeightG;
  result.emptySpoolWeightGrams = definition.emptySpoolWeightG;
  return TagImportValidationError::None;
}

const char* tagImportValidationMessage(TagImportValidationError error) {
  switch (error) {
    case TagImportValidationError::None: return "";
    case TagImportValidationError::UnsupportedFormat: return "Dieses Tagformat kann nicht importiert werden";
    case TagImportValidationError::MissingVendor: return "Der Tag enth\xC3\xA4lt keinen Hersteller";
    case TagImportValidationError::MissingMaterial: return "Der Tag enth\xC3\xA4lt kein Material";
    case TagImportValidationError::MissingFilament: return "Der Tag enth\xC3\xA4lt keinen Filamentnamen";
    case TagImportValidationError::MissingColor: return "Der Tag enth\xC3\xA4lt keine g\xC3\xBCltige Farbe";
    case TagImportValidationError::MissingWeight: return "Der Tag enth\xC3\xA4lt kein g\xC3\xBCltiges Gewicht";
    case TagImportValidationError::UnsupportedMaterial: return "F\xC3\xBCr dieses Material ist keine sichere Dichte hinterlegt";
    case TagImportValidationError::InvalidColor: return "Der Farbcode im Tag ist ung\xC3\xBCltig";
    case TagImportValidationError::InvalidWeight: return "Das Leergewicht im Tag ist ung\xC3\xBCltig";
    case TagImportValidationError::InvalidTemperature: return "Der Temperaturbereich im Tag ist ung\xC3\xBCltig";
  }
  return "Unbekannter Importfehler";
}

}  // namespace services
}  // namespace filament_station
