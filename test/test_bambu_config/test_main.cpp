#include <unity.h>

#include <cstdio>

#include "models/BambuPrinterConfig.h"

using namespace filament_station::models;

void setUp() {}
void tearDown() {}

namespace {

BambuConfigCollection validCollection() {
  BambuConfigCollection collection{};
  collection.printerCount = 2;
  collection.defaultPrinterId = 20;
  collection.selectedPrinterId = 10;

  collection.printers[0].printerId = 10;
  std::snprintf(collection.printers[0].name, sizeof(collection.printers[0].name),
                "Drucker 1");
  std::snprintf(collection.printers[0].host, sizeof(collection.printers[0].host),
                "192.168.10.50");
  std::snprintf(collection.printers[0].serialNumber,
                sizeof(collection.printers[0].serialNumber), "01P00A123456789");
  std::snprintf(collection.printers[0].accessCode,
                sizeof(collection.printers[0].accessCode), "12345678");
  collection.printers[0].enabled = true;
  collection.printers[0].isSelected = true;

  collection.printers[1].printerId = 20;
  std::snprintf(collection.printers[1].name, sizeof(collection.printers[1].name),
                "Drucker 2");
  std::snprintf(collection.printers[1].host, sizeof(collection.printers[1].host),
                "192.168.10.51");
  std::snprintf(collection.printers[1].serialNumber,
                sizeof(collection.printers[1].serialNumber), "01P00A987654321");
  std::snprintf(collection.printers[1].accessCode,
                sizeof(collection.printers[1].accessCode), "87654321");
  collection.printers[1].enabled = true;
  collection.printers[1].isDefault = true;

  return collection;
}

void testUniqueIdsAndLookup() {
  const BambuConfigCollection collection = validCollection();
  TEST_ASSERT_TRUE(hasUniqueBambuPrinterIds(collection));
  TEST_ASSERT_EQUAL_PTR(&collection.printers[0], findPrinterConfig(collection, 10));
  TEST_ASSERT_EQUAL_PTR(&collection.printers[1], findPrinterConfig(collection, 20));
  TEST_ASSERT_NULL(findPrinterConfig(collection, kInvalidPrinterId));
  TEST_ASSERT_TRUE(isValidBambuConfigCollection(collection));
}

void testDuplicateIdsAreRejected() {
  BambuConfigCollection collection = validCollection();
  collection.printers[1].printerId = 10;
  TEST_ASSERT_FALSE(hasUniqueBambuPrinterIds(collection));
  TEST_ASSERT_FALSE(isValidBambuConfigCollection(collection));
}

void testExactlyOneDefaultIsRequired() {
  BambuConfigCollection collection = validCollection();
  collection.printers[1].isDefault = false;
  TEST_ASSERT_FALSE(isValidBambuConfigCollection(collection));

  collection = validCollection();
  collection.printers[0].isDefault = true;
  TEST_ASSERT_FALSE(isValidBambuConfigCollection(collection));
}

void testSelectionIsOptionalButMustBeConsistent() {
  BambuConfigCollection collection = validCollection();
  collection.printers[0].isSelected = false;
  collection.selectedPrinterId = kInvalidPrinterId;
  TEST_ASSERT_TRUE(isValidBambuConfigCollection(collection));

  collection = validCollection();
  collection.selectedPrinterId = 20;
  TEST_ASSERT_FALSE(isValidBambuConfigCollection(collection));
}

void testEmptyCollectionHasNoDefaultOrSelection() {
  BambuConfigCollection collection{};
  TEST_ASSERT_TRUE(isValidBambuConfigCollection(collection));

  collection.defaultPrinterId = 5;
  TEST_ASSERT_FALSE(isValidBambuConfigCollection(collection));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testUniqueIdsAndLookup);
  RUN_TEST(testDuplicateIdsAreRejected);
  RUN_TEST(testExactlyOneDefaultIsRequired);
  RUN_TEST(testSelectionIsOptionalButMustBeConsistent);
  RUN_TEST(testEmptyCollectionHasNoDefaultOrSelection);
  return UNITY_END();
}
