#include "ui/models/MockUiDataProvider.h"

namespace filament_station::ui::models::mock {
namespace {

constexpr std::array<UiPrinterSummary, 3> kPrinters{{
    {1, "P1S Werkstatt", UiConnectionState::Connected, 1, 1, true, true, true},
    {2, "X1C Labor", UiConnectionState::Connected, 2, 2, false, false, true},
    {3, "A1 Mini Buero", UiConnectionState::Offline, 0, 0, false, false, true},
}};

constexpr std::array<UiAmsSummary, 4> kAmsUnits{{
    {1, 1, 3, 4, true, UiConnectionState::Connected},
    {2, 1, 2, 4, false, UiConnectionState::Connected},
    {2, 2, 4, 4, true, UiConnectionState::Connected},
    {3, 0, 0, 0, false, UiConnectionState::Offline},
}};

constexpr std::array<UiTraySummary, 9> kTrays{{
    {1, 1, 0, 42, UiTrayState::Loaded, 0xF5F5F5, 642.0F, "PLA", false, true, true},
    {1, 1, 1, 51, UiTrayState::Ready, 0xE53935, 811.0F, "PETG", false, true, false},
    {1, 1, 2, 67, UiTrayState::Ready, 0x1E88E5, 504.0F, "PLA", false, true, false},
    {1, 1, 3, 0, UiTrayState::Empty, 0x616161, 0.0F, "", false, false, false},
    {1, 0xFF, 0xFF, 73, UiTrayState::Ready, 0x43A047, 730.0F, "TPU", true, true, false},
    {2, 2, 0, 81, UiTrayState::Ready, 0x8E24AA, 920.0F, "PLA", false, true, false},
    {2, 2, 1, 82, UiTrayState::Loaded, 0xFB8C00, 377.0F, "ABS", false, true, true},
    {2, 2, 2, 83, UiTrayState::Ready, 0x00ACC1, 756.0F, "PETG", false, true, false},
    {2, 2, 3, 84, UiTrayState::Ready, 0x212121, 488.0F, "ASA", false, true, false},
}};

constexpr UiStagingSummary kStaging{
    1, 91, UiStagingState::WeightReady, 0xFDD835, 1247.0F, 997.0F,
    "Bambu Lab", "PLA", "Tag erkannt"};

constexpr UiSpoolSummary kSpool{
    91, "Bambu Lab", "PLA Basic", "PLA", 0xFDD835,
    250.0F, 1000.0F, 1247.0F, 997.0F, false};

constexpr UiWeightState kWeight{
    1247.0F, 997.0F, true, true, false, "stabil"};

constexpr UiSettingsState kSettings{
    "FilamentStation", "spoolman.local", "Werkstatt", "192.168.1.80",
    1, 1, UiConnectionState::Connected, UiConnectionState::Connected, true};

}  // namespace

const std::array<UiPrinterSummary, 3>& printers() { return kPrinters; }
const std::array<UiAmsSummary, 4>& amsUnits() { return kAmsUnits; }
const std::array<UiTraySummary, 9>& trays() { return kTrays; }
const UiStagingSummary& staging() { return kStaging; }
const UiSpoolSummary& spool() { return kSpool; }
const UiWeightState& weight() { return kWeight; }
const UiSettingsState& settings() { return kSettings; }

const UiPrinterSummary* findPrinter(rtos::PrinterId printerId) {
  for (const auto& printer : kPrinters) {
    if (printer.printerId == printerId) {
      return &printer;
    }
  }
  return nullptr;
}

const UiAmsSummary* findAms(rtos::PrinterId printerId, std::uint8_t amsId) {
  for (const auto& ams : kAmsUnits) {
    if (ams.printerId == printerId && ams.amsId == amsId) {
      return &ams;
    }
  }
  return nullptr;
}

const UiTraySummary* findTray(rtos::PrinterId printerId, std::uint8_t amsId,
                             std::uint8_t trayId) {
  for (const auto& tray : kTrays) {
    if (tray.printerId == printerId && tray.amsId == amsId &&
        tray.trayId == trayId) {
      return &tray;
    }
  }
  return nullptr;
}

}  // namespace filament_station::ui::models::mock
