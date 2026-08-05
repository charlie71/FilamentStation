#pragma once

#include <array>

#include "ui/models/UiModels.h"

namespace filament_station::ui::models::mock {

const std::array<UiPrinterSummary, 3>& printers();
const std::array<UiAmsSummary, 4>& amsUnits();
const std::array<UiTraySummary, 9>& trays();
const UiStagingSummary& staging();
const UiSpoolSummary& spool();
const UiWeightState& weight();
const UiSettingsState& settings();

const UiPrinterSummary* findPrinter(rtos::PrinterId printerId);
const UiAmsSummary* findAms(rtos::PrinterId printerId, std::uint8_t amsId);
const UiTraySummary* findTray(rtos::PrinterId printerId, std::uint8_t amsId,
                             std::uint8_t trayId);

}  // namespace filament_station::ui::models::mock
