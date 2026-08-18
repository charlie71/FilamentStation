#pragma once

#include "models/SpoolmanCatalog.h"

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

}  // namespace services
}  // namespace filament_station
