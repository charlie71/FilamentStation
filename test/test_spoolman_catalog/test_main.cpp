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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testVendorValidationAndDuplicateNormalization);
  RUN_TEST(testFilamentValidation);
  RUN_TEST(testFilamentDuplicateKeyUsesVendorNameMaterialAndColor);
  return UNITY_END();
}
