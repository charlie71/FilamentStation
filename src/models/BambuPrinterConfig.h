#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "models/PrinterState.h"

namespace filament_station {
namespace models {

// Persisted /config/bambu.json entry for a single Bambu printer.
// Distinct from PrinterState, which holds runtime connection/AMS status
// rather than configured connection data.
struct BambuPrinterConfig {
  PrinterId printerId = kInvalidPrinterId;
  char name[32]{};
  char host[40]{};
  char serialNumber[24]{};
  char accessCode[24]{};
  bool enabled = false;
  bool isDefault = false;
  bool isSelected = false;
};

struct BambuConfigCollection {
  std::array<BambuPrinterConfig, kMaximumPrinters> printers{};
  std::uint8_t printerCount = 0;
  PrinterId selectedPrinterId = kInvalidPrinterId;
  PrinterId defaultPrinterId = kInvalidPrinterId;
};

inline BambuPrinterConfig* findPrinterConfig(BambuConfigCollection& collection,
                                             PrinterId printerId) {
  if (!isValidPrinterId(printerId)) return nullptr;
  const std::size_t count = collection.printerCount < kMaximumPrinters
                                ? collection.printerCount
                                : kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index) {
    if (collection.printers[index].printerId == printerId)
      return &collection.printers[index];
  }
  return nullptr;
}

inline const BambuPrinterConfig* findPrinterConfig(
    const BambuConfigCollection& collection, PrinterId printerId) {
  if (!isValidPrinterId(printerId)) return nullptr;
  const std::size_t count = collection.printerCount < kMaximumPrinters
                                ? collection.printerCount
                                : kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index) {
    if (collection.printers[index].printerId == printerId)
      return &collection.printers[index];
  }
  return nullptr;
}

inline bool hasUniqueBambuPrinterIds(const BambuConfigCollection& collection) {
  if (collection.printerCount > kMaximumPrinters) return false;
  for (std::size_t left = 0; left < collection.printerCount; ++left) {
    const PrinterId id = collection.printers[left].printerId;
    if (!isValidPrinterId(id)) return false;
    for (std::size_t right = left + 1; right < collection.printerCount;
         ++right) {
      if (collection.printers[right].printerId == id) return false;
    }
  }
  return true;
}

// A configured default printer is mandatory once printers exist; an
// explicit selection is optional (runtime may fall back to the default
// printer until the user picks one).
inline bool isValidBambuConfigCollection(
    const BambuConfigCollection& collection) {
  if (!hasUniqueBambuPrinterIds(collection)) return false;
  if (collection.printerCount == 0) {
    return collection.selectedPrinterId == kInvalidPrinterId &&
           collection.defaultPrinterId == kInvalidPrinterId;
  }

  const BambuPrinterConfig* defaultPrinter =
      findPrinterConfig(collection, collection.defaultPrinterId);
  if (defaultPrinter == nullptr || !defaultPrinter->isDefault) return false;

  if (collection.selectedPrinterId != kInvalidPrinterId) {
    const BambuPrinterConfig* selectedPrinter =
        findPrinterConfig(collection, collection.selectedPrinterId);
    if (selectedPrinter == nullptr || !selectedPrinter->isSelected)
      return false;
  }

  std::size_t defaultCount = 0;
  std::size_t selectedCount = 0;
  for (std::size_t index = 0; index < collection.printerCount; ++index) {
    const BambuPrinterConfig& printer = collection.printers[index];
    if (printer.isDefault) ++defaultCount;
    if (printer.isSelected) ++selectedCount;
  }
  if (defaultCount != 1) return false;
  if (collection.selectedPrinterId == kInvalidPrinterId) {
    return selectedCount == 0;
  }
  return selectedCount == 1;
}

static_assert(std::is_trivially_copyable<BambuPrinterConfig>::value,
              "BambuPrinterConfig must be trivially copyable");
static_assert(std::is_trivially_copyable<BambuConfigCollection>::value,
              "BambuConfigCollection must be trivially copyable");

}  // namespace models
}  // namespace filament_station
