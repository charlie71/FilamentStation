#pragma once

#include "models/SpoolmanCatalog.h"
#include "models/TagDefinition.h"

namespace filament_station {
namespace services {

enum class CatalogValidationError {
  None,
  MissingName,
  MissingMaterial,
  MissingVendor,
  InvalidDensity,
  InvalidDiameter,
  InvalidWeight,
  InvalidSpoolWeight,
  InvalidColor,
  InvalidTemperature,
};

CatalogValidationError validateVendor(const models::SpoolmanVendor& vendor);
CatalogValidationError validateFilament(
    const models::SpoolmanFilament& filament);
const char* catalogValidationMessage(CatalogValidationError error);
bool sameVendor(const models::SpoolmanVendor& left,
                const models::SpoolmanVendor& right);
bool sameFilament(const models::SpoolmanFilament& left,
                  const models::SpoolmanFilament& right);

enum class WeightUpdateValidationError : unsigned char {
  None,
  MissingSpool,
  InvalidRemainingWeight,
  InvalidInitialWeight,
  InvalidEmptySpoolWeight,
};

WeightUpdateValidationError validateWeightUpdate(
    const models::SpoolmanWeightUpdate& update);

enum class TagImportValidationError : unsigned char {
  None,
  UnsupportedFormat,
  MissingVendor,
  MissingMaterial,
  MissingFilament,
  MissingColor,
  MissingWeight,
  UnsupportedMaterial,
  InvalidColor,
  InvalidWeight,
  InvalidTemperature,
};

TagImportValidationError mapTagDefinition(
    const models::TagDefinition& definition,
    models::SpoolmanImportDefinition& result);
const char* tagImportValidationMessage(TagImportValidationError error);

}  // namespace services
}  // namespace filament_station
