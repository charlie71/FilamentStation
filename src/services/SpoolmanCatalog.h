/**
 * @file
 * @brief Validation and equality-comparison helpers for the vendor/filament
 *        catalog and weight-update payloads, plus the tag-definition ->
 *        Spoolman-import mapping (models::TagDefinition ->
 *        models::SpoolmanImportDefinition).
 */
#pragma once

#include "models/SpoolmanCatalog.h"
#include "models/TagDefinition.h"

namespace filament_station {
namespace services {

/// @brief Result of validateVendor()/validateFilament().
enum class CatalogValidationError {
  None,               ///< Valid.
  MissingName,        ///< Vendor/filament name is empty.
  MissingMaterial,    ///< Filament material is empty.
  MissingVendor,      ///< Filament's vendor reference is empty/missing.
  InvalidDensity,     ///< Filament density is non-finite or out of range.
  InvalidDiameter,    ///< Filament diameter is non-finite or out of range.
  InvalidWeight,      ///< Filament nominal weight is non-finite or out of range.
  InvalidSpoolWeight, ///< Filament empty-spool weight is non-finite or out of range.
  InvalidColor,       ///< Color code is not a valid "#RRGGBB"/"#RRGGBBAA" string.
  InvalidTemperature, ///< Nozzle temperature range is invalid (min > max, or out of range).
};

/// @brief Validates a vendor entry before creating/comparing it on Spoolman.
/// @param vendor Vendor to validate.
/// @return CatalogValidationError::None if valid, otherwise the first violation found.
CatalogValidationError validateVendor(const models::SpoolmanVendor& vendor);
/// @brief Validates a filament entry before creating/comparing it on Spoolman.
/// @param filament Filament to validate.
/// @return CatalogValidationError::None if valid, otherwise the first violation found.
CatalogValidationError validateFilament(
    const models::SpoolmanFilament& filament);
/// @brief Human-readable message for a CatalogValidationError.
/// @param error Error code to describe.
/// @return Static, NUL-terminated description string.
const char* catalogValidationMessage(CatalogValidationError error);
/// @brief Compares two vendors for the fields that determine catalog identity.
/// @param left First vendor.
/// @param right Second vendor.
/// @return true if they represent the same vendor.
bool sameVendor(const models::SpoolmanVendor& left,
                const models::SpoolmanVendor& right);
/// @brief Compares two filaments for the fields that determine catalog identity.
/// @param left First filament.
/// @param right Second filament.
/// @return true if they represent the same filament.
bool sameFilament(const models::SpoolmanFilament& left,
                  const models::SpoolmanFilament& right);

/// @brief Result of validateWeightUpdate().
enum class WeightUpdateValidationError : unsigned char {
  None,                    ///< Valid.
  MissingSpool,            ///< Spool id is 0.
  InvalidRemainingWeight,  ///< Remaining weight is non-finite or negative.
  InvalidInitialWeight,    ///< Initial weight is non-finite or negative.
  InvalidEmptySpoolWeight, ///< Empty-spool weight is non-finite or negative.
};

/// @brief Validates a weight-update payload before sending it to Spoolman.
/// @param update Weight update to validate.
/// @return WeightUpdateValidationError::None if valid, otherwise the first violation found.
WeightUpdateValidationError validateWeightUpdate(
    const models::SpoolmanWeightUpdate& update);

/// @brief Result of mapTagDefinition().
enum class TagImportValidationError : unsigned char {
  None,                 ///< Valid; `result` was filled in.
  UnsupportedFormat,    ///< The tag's format cannot be imported (e.g. Unknown).
  MissingVendor,        ///< Tag definition has no usable vendor name.
  MissingMaterial,      ///< Tag definition has no usable material name.
  MissingFilament,      ///< Tag definition has no usable filament name.
  MissingColor,         ///< Tag definition has no usable color code.
  MissingWeight,        ///< Tag definition has no usable nominal weight.
  UnsupportedMaterial,  ///< Material is not one Spoolman recognizes for import.
  InvalidColor,         ///< Color code is not a valid "#RRGGBB" string.
  InvalidWeight,        ///< Nominal/empty-spool weight is non-finite or out of range.
  InvalidTemperature,   ///< Nozzle temperature range is invalid.
};

/// @brief Maps a decoded tag definition to a Spoolman import request.
/// @param definition Parsed tag definition (from nfc::TagParserRegistry).
/// @param result Out parameter receiving the mapped vendor/filament/spool fields.
/// @return TagImportValidationError::None if the mapping succeeded, otherwise the first violation found.
TagImportValidationError mapTagDefinition(
    const models::TagDefinition& definition,
    models::SpoolmanImportDefinition& result);
/// @brief Human-readable message for a TagImportValidationError.
/// @param error Error code to describe.
/// @return Static, NUL-terminated description string.
const char* tagImportValidationMessage(TagImportValidationError error);

}  // namespace services
}  // namespace filament_station
