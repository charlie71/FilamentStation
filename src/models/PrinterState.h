#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

using PrinterId = std::uint16_t;

constexpr PrinterId kInvalidPrinterId = 0;
constexpr std::size_t kMaximumPrinters = 4;
constexpr std::size_t kMaximumAmsPerPrinter = 4;
constexpr std::size_t kSlotsPerAms = 4;

enum class PrinterConnectionState : std::uint8_t {
  Disabled,
  Offline,
  Connecting,
  Connected,
  Error,
};

enum class AmsConnectionState : std::uint8_t {
  Unavailable,
  Offline,
  Connecting,
  Connected,
  Error,
};

enum class PrinterSlotState : std::uint8_t {
  Unknown,
  Empty,
  Ready,
  Loading,
  Loaded,
  Unloading,
  Error,
};

struct PrinterSlotStateData {
  std::uint8_t trayId = 0;
  std::uint32_t spoolId = 0;
  PrinterSlotState state = PrinterSlotState::Unknown;
  // Printer-reported material/color for this tray (Bambu tray_type/
  // tray_color), not a Spoolman identity -- the printer has no notion of
  // Spoolman spool IDs, see docs/bambu-protocol.md.
  char material[12]{};
  char colorHex[9]{};
};

struct AmsState {
  std::uint8_t amsId = 0;
  bool present = false;
  AmsConnectionState connectionState = AmsConnectionState::Unavailable;
  std::array<PrinterSlotStateData, kSlotsPerAms> slots{};
};

struct PrinterState {
  PrinterId printerId = kInvalidPrinterId;
  char name[32]{};
  bool enabled = false;
  bool isDefault = false;
  bool isActive = false;
  PrinterConnectionState connectionState = PrinterConnectionState::Disabled;
  std::uint8_t activeAmsId = 0;
  std::uint8_t amsCount = 0;
  std::array<AmsState, kMaximumAmsPerPrinter> amsUnits{};
  PrinterSlotStateData externalSlot{};
  // Printer-reported nozzle diameter (e.g. "0.4"), from "print.nozzle_diameter"
  // in status reports. Empty until the first report with that field arrives;
  // required (as a string) by the "extrusion_cali_sel" wire command, see
  // docs/bambu-protocol.md.
  char nozzleDiameter[8]{};
};

struct PrinterStateCollection {
  std::array<PrinterState, kMaximumPrinters> printers{};
  std::uint8_t printerCount = 0;
  PrinterId activePrinterId = kInvalidPrinterId;
  PrinterId defaultPrinterId = kInvalidPrinterId;
};

constexpr bool isValidPrinterId(PrinterId printerId) {
  return printerId != kInvalidPrinterId;
}

inline PrinterState* findPrinter(PrinterStateCollection& collection,
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

inline const PrinterState* findPrinter(
    const PrinterStateCollection& collection, PrinterId printerId) {
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

inline AmsState* findAms(PrinterState& printer, std::uint8_t amsId) {
  for (auto& ams : printer.amsUnits) {
    if (ams.present && ams.amsId == amsId) return &ams;
  }
  return nullptr;
}

inline const AmsState* findAms(const PrinterState& printer,
                              std::uint8_t amsId) {
  for (const auto& ams : printer.amsUnits) {
    if (ams.present && ams.amsId == amsId) return &ams;
  }
  return nullptr;
}

inline PrinterSlotStateData* findSlot(PrinterState& printer,
                                      std::uint8_t amsId,
                                      std::uint8_t trayId) {
  AmsState* ams = findAms(printer, amsId);
  if (ams == nullptr || trayId >= kSlotsPerAms) return nullptr;
  return &ams->slots[trayId];
}

inline bool hasUniquePrinterIds(const PrinterStateCollection& collection) {
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

inline bool isValidPrinterState(const PrinterStateCollection& collection) {
  if (!hasUniquePrinterIds(collection)) return false;
  if (collection.printerCount == 0)
    return collection.activePrinterId == kInvalidPrinterId &&
           collection.defaultPrinterId == kInvalidPrinterId;

  const PrinterState* active = findPrinter(collection, collection.activePrinterId);
  const PrinterState* defaultPrinter =
      findPrinter(collection, collection.defaultPrinterId);
  if (active == nullptr || defaultPrinter == nullptr || !active->isActive ||
      !defaultPrinter->isDefault)
    return false;

  std::size_t activeCount = 0;
  std::size_t defaultCount = 0;
  for (std::size_t index = 0; index < collection.printerCount; ++index) {
    const PrinterState& printer = collection.printers[index];
    if (printer.amsCount > kMaximumAmsPerPrinter) return false;
    if (printer.isActive) ++activeCount;
    if (printer.isDefault) ++defaultCount;
    if (printer.amsCount > 0 && findAms(printer, printer.activeAmsId) == nullptr)
      return false;
  }
  return activeCount == 1 && defaultCount == 1;
}

static_assert(std::is_trivially_copyable<PrinterSlotStateData>::value,
              "PrinterSlotStateData must be trivially copyable");
static_assert(std::is_trivially_copyable<AmsState>::value,
              "AmsState must be trivially copyable");
static_assert(std::is_trivially_copyable<PrinterState>::value,
              "PrinterState must be trivially copyable");
static_assert(std::is_trivially_copyable<PrinterStateCollection>::value,
              "PrinterStateCollection must be trivially copyable");

}  // namespace models
}  // namespace filament_station
