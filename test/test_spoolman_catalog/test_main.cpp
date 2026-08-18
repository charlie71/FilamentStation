#include <unity.h>

#include <cstdio>
#include "services/SpoolmanCatalog.h"

using filament_station::models::SpoolmanFilament;
using filament_station::models::SpoolmanVendor;
using filament_station::services::CatalogValidationError;

void setUp() {}
void tearDown() {}

SpoolmanFilament validFilament() {
  SpoolmanFilament filament{};
  filament.vendorId = 7;
  std::snprintf(filament.name, sizeof(filament.name), "PLA Basic Green");
  std::snprintf(filament.material, sizeof(filament.material), "PLA");
  std::snprintf(filament.colorHex, sizeof(filament.colorHex), "00AE42");
  filament.densityGramsPerCm3 = 1.24F;
  filament.diameterMillimeters = 1.75F;
  filament.weightGrams = 1000.0F;
  return filament;
}

void testVendorValidationAndDuplicateNormalization() {
  SpoolmanVendor first{};
  SpoolmanVendor second{};
  std::snprintf(first.name, sizeof(first.name), " Bambu Lab ");
  std::snprintf(second.name, sizeof(second.name), "bambu lab");
  TEST_ASSERT_EQUAL(CatalogValidationError::None,
                    filament_station::services::validateVendor(first));
  TEST_ASSERT_TRUE(filament_station::services::sameVendor(first, second));
  second.name[0] = '\0';
  TEST_ASSERT_EQUAL(CatalogValidationError::MissingName,
                    filament_station::services::validateVendor(second));
}

void testFilamentValidation() {
  SpoolmanFilament filament = validFilament();
  TEST_ASSERT_EQUAL(CatalogValidationError::None,
                    filament_station::services::validateFilament(filament));
  filament.densityGramsPerCm3 = 0.0F;
  TEST_ASSERT_EQUAL(CatalogValidationError::InvalidDensity,
                    filament_station::services::validateFilament(filament));
  filament = validFilament();
  std::snprintf(filament.colorHex, sizeof(filament.colorHex), "GG0000");
  TEST_ASSERT_EQUAL(CatalogValidationError::InvalidColor,
                    filament_station::services::validateFilament(filament));
}

void testFilamentDuplicateKeyUsesVendorNameMaterialAndColor() {
  SpoolmanFilament first = validFilament();
  SpoolmanFilament second = validFilament();
  std::snprintf(second.name, sizeof(second.name), " pla basic green ");
  std::snprintf(second.material, sizeof(second.material), "pla");
  std::snprintf(second.colorHex, sizeof(second.colorHex), "#00ae42");
  TEST_ASSERT_TRUE(filament_station::services::sameFilament(first, second));
  second.vendorId = 8;
  TEST_ASSERT_FALSE(filament_station::services::sameFilament(first, second));
}

void testTagDefinitionMapping() {
  filament_station::models::TagDefinition definition{};
  definition.format = filament_station::models::TagFormat::OpenTag3D;
  std::snprintf(definition.vendor, sizeof(definition.vendor), "Example");
  std::snprintf(definition.filamentName, sizeof(definition.filamentName),
                "PLA Red");
  std::snprintf(definition.material, sizeof(definition.material), "PLA");
  std::snprintf(definition.colorCode, sizeof(definition.colorCode), "#AABBCC");
  definition.nominalFilamentWeightG = 1000.0F;
  definition.emptySpoolWeightG = 245.0F;
  definition.nozzleTempMinC = 190;
  definition.nozzleTempMaxC = 220;
  filament_station::models::SpoolmanImportDefinition mapped{};
  TEST_ASSERT_EQUAL(
      filament_station::services::TagImportValidationError::None,
      filament_station::services::mapTagDefinition(definition, mapped));
  TEST_ASSERT_EQUAL_STRING("Example", mapped.vendor.name);
  TEST_ASSERT_EQUAL_STRING("PLA", mapped.filament.material);
  TEST_ASSERT_EQUAL_STRING("AABBCC", mapped.filament.colorHex);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.24F,
                           mapped.filament.densityGramsPerCm3);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.75F,
                           mapped.filament.diameterMillimeters);
  TEST_ASSERT_EQUAL_INT(205, mapped.filament.nozzleTemperatureC);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 1000.0F, mapped.initialWeightGrams);
}

void testTagDefinitionValidationAndFormats() {
  const filament_station::models::TagFormat formats[] = {
      filament_station::models::TagFormat::BambuLab,
      filament_station::models::TagFormat::OpenPrintTag,
      filament_station::models::TagFormat::OpenTag3D,
      filament_station::models::TagFormat::Legacy};
  for (const auto format : formats) {
    filament_station::models::TagDefinition definition{};
    definition.format = format;
    std::snprintf(definition.vendor, sizeof(definition.vendor), "Vendor");
    std::snprintf(definition.filamentName, sizeof(definition.filamentName),
                  "PETG Blue");
    std::snprintf(definition.material, sizeof(definition.material), "PETG");
    std::snprintf(definition.colorCode, sizeof(definition.colorCode), "112233");
    definition.nominalFilamentWeightG = 1000.0F;
    filament_station::models::SpoolmanImportDefinition mapped{};
    TEST_ASSERT_EQUAL(
        filament_station::services::TagImportValidationError::None,
        filament_station::services::mapTagDefinition(definition, mapped));
  }
  filament_station::models::TagDefinition missing{};
  missing.format = filament_station::models::TagFormat::BambuLab;
  filament_station::models::SpoolmanImportDefinition mapped{};
  TEST_ASSERT_EQUAL(
      filament_station::services::TagImportValidationError::MissingVendor,
      filament_station::services::mapTagDefinition(missing, mapped));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testVendorValidationAndDuplicateNormalization);
  RUN_TEST(testFilamentValidation);
  RUN_TEST(testFilamentDuplicateKeyUsesVendorNameMaterialAndColor);
  RUN_TEST(testTagDefinitionMapping);
  RUN_TEST(testTagDefinitionValidationAndFormats);
  return UNITY_END();
}
