#include <unity.h>

#include <cstdio>

#include "models/PrinterState.h"

using namespace filament_station::models;

void setUp() {}
void tearDown() {}

namespace {

PrinterStateCollection validCollection() {
  PrinterStateCollection collection{};
  collection.printerCount = 2;
  collection.activePrinterId = 10;
  collection.defaultPrinterId = 20;

  collection.printers[0].printerId = 10;
  collection.printers[0].enabled = true;
  collection.printers[0].isActive = true;
  collection.printers[0].connectionState =
      PrinterConnectionState::Connected;
  collection.printers[0].activeAmsId = 1;
  collection.printers[0].amsCount = 1;
  collection.printers[0].amsUnits[0].amsId = 1;
  collection.printers[0].amsUnits[0].present = true;
  collection.printers[0].amsUnits[0].connectionState =
      AmsConnectionState::Connected;
  for (std::uint8_t tray = 0; tray < kSlotsPerAms; ++tray)
    collection.printers[0].amsUnits[0].slots[tray].trayId = tray;

  collection.printers[1].printerId = 20;
  collection.printers[1].enabled = true;
  collection.printers[1].isDefault = true;
  collection.printers[1].connectionState = PrinterConnectionState::Offline;
  return collection;
}

void testMultiplePrintersAndStableIds() {
  const PrinterStateCollection collection = validCollection();
  TEST_ASSERT_TRUE(hasUniquePrinterIds(collection));
  TEST_ASSERT_EQUAL_PTR(&collection.printers[0], findPrinter(collection, 10));
  TEST_ASSERT_EQUAL_PTR(&collection.printers[1], findPrinter(collection, 20));
  TEST_ASSERT_NULL(findPrinter(collection, kInvalidPrinterId));
  TEST_ASSERT_TRUE(isValidPrinterState(collection));
}

void testDuplicateAndMissingIdsAreRejected() {
  PrinterStateCollection collection = validCollection();
  collection.printers[1].printerId = 10;
  TEST_ASSERT_FALSE(hasUniquePrinterIds(collection));
  TEST_ASSERT_FALSE(isValidPrinterState(collection));

  collection = validCollection();
  collection.printers[0].printerId = kInvalidPrinterId;
  TEST_ASSERT_FALSE(hasUniquePrinterIds(collection));
}

void testActiveAndDefaultPrinterConsistency() {
  PrinterStateCollection collection = validCollection();
  collection.printers[0].isActive = false;
  TEST_ASSERT_FALSE(isValidPrinterState(collection));

  collection = validCollection();
  collection.printers[1].isDefault = false;
  TEST_ASSERT_FALSE(isValidPrinterState(collection));
}

void testAmsAndSlotLookup() {
  PrinterStateCollection collection = validCollection();
  PrinterState* printer = findPrinter(collection, 10);
  TEST_ASSERT_NOT_NULL(printer);
  TEST_ASSERT_NOT_NULL(findAms(*printer, 1));
  TEST_ASSERT_NULL(findAms(*printer, 2));

  PrinterSlotStateData* slot = findSlot(*printer, 1, 3);
  TEST_ASSERT_NOT_NULL(slot);
  std::snprintf(slot->material, sizeof(slot->material), "PLA");
  slot->state = PrinterSlotState::Ready;
  TEST_ASSERT_EQUAL_STRING("PLA", printer->amsUnits[0].slots[3].material);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Ready),
                        static_cast<int>(printer->amsUnits[0].slots[3].state));
  TEST_ASSERT_NULL(findSlot(*printer, 1, kSlotsPerAms));
}

void testActiveAmsMustExist() {
  PrinterStateCollection collection = validCollection();
  collection.printers[0].activeAmsId = 4;
  TEST_ASSERT_FALSE(isValidPrinterState(collection));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testMultiplePrintersAndStableIds);
  RUN_TEST(testDuplicateAndMissingIdsAreRejected);
  RUN_TEST(testActiveAndDefaultPrinterConsistency);
  RUN_TEST(testAmsAndSlotLookup);
  RUN_TEST(testActiveAmsMustExist);
  return UNITY_END();
}
