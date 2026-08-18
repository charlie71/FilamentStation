#include "services/SpoolmanCatalog.h"

#include <cctype>
#include <cstring>
#include <cstdio>

namespace filament_station {
namespace services {
namespace {

const char* skipSpaces(const char* value) {
  while (*value != '\0' && std::isspace(static_cast<unsigned char>(*value)))
    ++value;
  return value;
}

bool isBlank(const char* value) {
  value = skipSpaces(value);
  return *value == '\0';
}

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
    case CatalogValidationError::InvalidDensity: return "Dichte muss groesser als null sein";
    case CatalogValidationError::InvalidDiameter: return "Durchmesser muss groesser als null sein";
    case CatalogValidationError::InvalidWeight: return "Filamentgewicht ist ungueltig";
    case CatalogValidationError::InvalidSpoolWeight: return "Leergewicht ist ungueltig";
    case CatalogValidationError::InvalidColor: return "Farbcode ist ungueltig";
    case CatalogValidationError::InvalidTemperature: return "Temperatur ist ungueltig";
  }
  return "Ungueltige Katalogdaten";
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

namespace {
float materialDensity(const char* material) {
  struct Entry { const char* name; float density; };
  static const Entry entries[] = {
      {"PLA", 1.24F}, {"PETG", 1.27F}, {"ABS", 1.04F},
      {"ASA", 1.07F}, {"TPU", 1.21F}, {"PA", 1.14F},
      {"PA6", 1.14F}, {"PA11", 1.04F}, {"PA12", 1.01F},
      {"PC", 1.20F}, {"PVA", 1.23F}, {"HIPS", 1.04F},
      {"PP", 0.90F}, {"PCTG", 1.23F}};
  for (const Entry& entry : entries)
    if (equalNormalized(material, entry.name)) return entry.density;
  return 0.0F;
}

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
    case TagImportValidationError::MissingVendor: return "Der Tag enthaelt keinen Hersteller";
    case TagImportValidationError::MissingMaterial: return "Der Tag enthaelt kein Material";
    case TagImportValidationError::MissingFilament: return "Der Tag enthaelt keinen Filamentnamen";
    case TagImportValidationError::MissingColor: return "Der Tag enthaelt keine gueltige Farbe";
    case TagImportValidationError::MissingWeight: return "Der Tag enthaelt kein gueltiges Gewicht";
    case TagImportValidationError::UnsupportedMaterial: return "Fuer dieses Material ist keine sichere Dichte hinterlegt";
    case TagImportValidationError::InvalidColor: return "Der Farbcode im Tag ist ungueltig";
    case TagImportValidationError::InvalidWeight: return "Das Leergewicht im Tag ist ungueltig";
    case TagImportValidationError::InvalidTemperature: return "Der Temperaturbereich im Tag ist ungueltig";
  }
  return "Unbekannter Importfehler";
}

}  // namespace services
}  // namespace filament_station
