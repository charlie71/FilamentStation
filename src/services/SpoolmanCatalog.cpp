#include "services/SpoolmanCatalog.h"

#include <cctype>
#include <cstring>

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

}  // namespace services
}  // namespace filament_station
