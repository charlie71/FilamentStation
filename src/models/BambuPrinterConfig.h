/**
 * @file
 * @brief Persisted Bambu Lab printer connection configuration
 *        (/config/bambu.json).
 */
#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "models/PrinterState.h"

namespace filament_station {
namespace models {

/// @brief One persisted printer's connection configuration.
///
/// Persisted /config/bambu.json entry for a single Bambu printer.
/// Distinct from PrinterState, which holds runtime connection/AMS status
/// rather than configured connection data.
struct BambuPrinterConfig {
  PrinterId printerId = kInvalidPrinterId;  ///< Locally assigned printer id.
  char name[32]{};             ///< Display name.
  char host[40]{};              ///< Hostname or IP address.
  char serialNumber[24]{};      ///< Printer serial number (MQTT topic/client id).
  char accessCode[24]{};        ///< LAN-mode MQTT access code (password).
  bool enabled = false;         ///< Whether connection attempts are permitted.
  bool isDefault = false;       ///< Whether this printer is selected on a fresh boot.
  bool isSelected = false;      ///< Whether this printer was explicitly selected by the user.
};

/// @brief All persisted printer configurations plus selection/default state.
struct BambuConfigCollection {
  std::array<BambuPrinterConfig, kMaximumPrinters> printers{};  ///< Configured printers.
  std::uint8_t printerCount = 0;  ///< Number of valid entries in #printers.
  PrinterId selectedPrinterId = kInvalidPrinterId;  ///< Explicitly user-selected printer, or kInvalidPrinterId.
  PrinterId defaultPrinterId = kInvalidPrinterId;   ///< Printer selected on a fresh boot.
};

/// @brief Finds a printer configuration by id.
/// @param collection Collection to search.
/// @param printerId Id to look up.
/// @return Pointer to the matching entry, or nullptr if not found/invalid id.
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

/// @brief Const overload of findPrinterConfig().
/// @param collection Collection to search.
/// @param printerId Id to look up.
/// @return Pointer to the matching entry, or nullptr if not found/invalid id.
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

/// @brief Validates that every printer in a collection has a distinct id.
/// @param collection Collection to check.
/// @return True if printerCount is within bounds and every id is unique and valid.
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

/// @brief Validates internal consistency of a whole BambuConfigCollection
///        (unique ids, exactly one default printer, at most one selected).
/// @param collection Collection to check.
/// @return True if the collection is internally consistent.
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
