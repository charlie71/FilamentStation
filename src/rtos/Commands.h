#pragma once

#include <cstdint>

namespace filament_station::rtos {

using PrinterId = std::uint16_t;
using SpoolId = std::uint32_t;

enum class UiScreenId : std::uint8_t {
  Boot,
  Home,
  PrinterSelect,
  SettingsHome,
  StagingDetails,
  StagingActions,
  TrayDetails,
  TrayActions,
  TraySelect,
  SettingsSpoolman,
  SettingsPrinters,
  SettingsPrinterEdit,
};

enum class UiCommandType : std::uint8_t {
  CommunicationTestResponse,
  ShowStatus,
  ShowScreen,
  ShowDialog,
  ShowProgress,
  HideProgress,
  UpdateHeader,
  UpdatePrinterList,
  UpdateAmsOverview,
  UpdateStaging,
  UpdateTrayDetails,
  UpdateSpoolDetails,
  UpdateWeight,
  UpdateSettings,
  UpdateBootStatus,
  ShowToast,
};

enum class UiActionType : std::uint8_t {
  SelectPrinter,
  SelectAms,
  SelectTray,
  SelectStaging,
  ClearStaging,
  OpenSettings,
  Back,
  Cancel,
  Confirm,
  Close,
  QuickWeight,
  AdvancedWeight,
  ConfigureSlot,
  ConfigureSlotFromStaging,
  ResetSlot,
  UntagSlot,
  ReapplySlot,
  RefreshSlot,
  LinkTag,
  WriteTag,
  EraseTag,
  UnlinkTag,
  SearchSpool,
  SelectSpool,
  SaveMeasurement,
  OpenWifiSettings,
  OpenSpoolmanSettings,
  OpenScaleSettings,
  OpenPrinterSettings,
  OpenDeviceSettings,
  OpenDiagnostics,
  OpenFirmwareSettings,
  AddPrinter,
  EditPrinter,
  DeletePrinter,
  TestPrinterConnection,
  SetDefaultPrinter,
  SetActivePrinter,
  TogglePrinterEnabled,
  SelectManagedPrinter,
  EditPrinterField,
  SavePrinterSettings,
  TestSpoolmanConnection,
  SaveSpoolmanSettings,
  EditSpoolmanSetting,
  StartWifiPortal,
  ResetWifiCredentials,
  TareScale,
  StartScaleCalibration,
  ResetScaleCalibration,
};

struct UiAction {
  UiActionType type = UiActionType::Back;
  std::uint32_t requestId = 0;
  PrinterId printerId = 0;
  SpoolId spoolId = 0;
  std::uint8_t amsId = 0;
  std::uint8_t trayId = 0;
  std::int32_t value = 0;
  char text[64]{};
};
enum class ScaleCommandType : std::uint8_t { Tare, StartCalibration, ResetCalibration, RequestMeasurement };
enum class NfcCommandType : std::uint8_t { StartReading, StopReading, WriteSpoolTag };
enum class StorageCommandType : std::uint8_t { LoadJson, SaveJson, DeleteJson, CreateBackup };
enum class NetworkCommandType : std::uint8_t { Connect, Reconfigure, StartPortal, ClearCredentials };
enum class SpoolmanCommandType : std::uint8_t { HealthCheck, LoadSpool, SearchSpools, UpdateWeight };
enum class BambuCommandType : std::uint8_t { Connect, Disconnect, RequestStatus, AssignTray };

}  // namespace filament_station::rtos
