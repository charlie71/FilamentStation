#pragma once

#include <cstdint>

namespace filament_station::rtos {

using PrinterId = std::uint16_t;
using SpoolId = std::uint32_t;

constexpr std::int32_t UI_TAG_CAP_WRITE = 1 << 0;
constexpr std::int32_t UI_TAG_CAP_LINK = 1 << 1;
constexpr std::int32_t UI_TAG_CAP_UNLINK = 1 << 2;
constexpr std::int32_t UI_TAG_CAP_ERASE = 1 << 3;

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
  SettingsWifi,
  SettingsScale,
  SettingsDevice,
  SettingsDiagnostics,
  SettingsFirmware,
  TagActionSelect,
  TagReview,
  TagWrite,
  TagResult,
  TagDefinitionImport,
  TagLegacy,
  TagUnknown,
  BambuSpoolType,
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
  UpdateNetworkStatus,
  UpdateSpoolmanState,
  UpdateSpoolPicker,
  ShowToast,
};

enum class UiNetworkState : std::uint8_t {
  Offline,
  Connecting,
  Online,
  PortalActive,
  CredentialsCleared,
};

enum class UiOverlayKind : std::uint8_t {
  None,
  BootProgress,
  ConnectionProgress,
  NfcRead,
  NfcWrite,
  WeightStabilizing,
  SpoolmanRequest,
  BambuConnection,
  BambuMappingSave,
  Confirmation,
  Error,
  Success,
  RestartConfirmation,
  WifiResetConfirmation,
  QuickWeightConfirmation,
  AdvancedWeightMode,
  AdvancedWeightInput,
  AdvancedWeightConfirmation,
  AdvancedWeightResult,
  TagActionSelect,
  TagReview,
  TagWrite,
  TagResult,
  SpoolPicker,
  TagDefinitionImport,
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
  AssignTag,
  RemoveTagAssignment,
  SearchSpool,
  SelectSpool,
  ImportTagDefinition,
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
  PrepareRestart,
  RefreshDiagnostics,
  CheckFirmwareUpdate,
};

constexpr bool isPublicTagAssignmentAction(UiActionType type) {
  return type == UiActionType::AssignTag ||
         type == UiActionType::RemoveTagAssignment;
}

constexpr bool requiresOnlineSpoolman(UiActionType type) {
  switch (type) {
    case UiActionType::AssignTag:
    case UiActionType::RemoveTagAssignment:
    case UiActionType::SearchSpool:
    case UiActionType::SelectSpool:
    case UiActionType::ImportTagDefinition:
    case UiActionType::SaveMeasurement:
    case UiActionType::QuickWeight:
    case UiActionType::AdvancedWeight:
    case UiActionType::ConfigureSlot:
    case UiActionType::ConfigureSlotFromStaging:
      return true;
    default:
      return false;
  }
}

static_assert(isPublicTagAssignmentAction(UiActionType::AssignTag));
static_assert(
    isPublicTagAssignmentAction(UiActionType::RemoveTagAssignment));
static_assert(requiresOnlineSpoolman(UiActionType::AssignTag));
static_assert(requiresOnlineSpoolman(UiActionType::QuickWeight));
static_assert(!requiresOnlineSpoolman(UiActionType::OpenSpoolmanSettings));

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
enum class ScaleCommandType : std::uint8_t {
  Tare,
  StartCalibration,
  ResetCalibration,
  RequestMeasurement,
  ApplyCalibration,
};
enum class NfcCommandType : std::uint8_t {
  StartReading,
  StopReading,
  WriteSpoolTag,
  EraseTag,
};
enum class StorageCommandType : std::uint8_t { LoadJson, SaveJson, DeleteJson, CreateBackup };
enum class NetworkCommandType : std::uint8_t {
  ApplyConfiguration,
  RequestStatus,
  Connect,
  Reconfigure,
  StartPortal,
  StopPortal,
  ClearCredentials,
};
enum class SpoolmanCommandType : std::uint8_t {
  ApplyConfiguration,
  HealthCheck,
  EnsureTagExtraField,
  FindSpoolByTag,
  SetSpoolTag,
  ClearSpoolTag,
  LoadSpool,
  SearchSpools,
  SearchVendors,
  CreateVendor,
  SearchFilaments,
  CreateFilament,
  UpdateWeight,
  ImportTagDefinition,
};
enum class SpoolmanSearchFilter : std::uint8_t {
  FilamentName,
  Material,
  Vendor,
  Id,
};
enum class BambuCommandType : std::uint8_t { Connect, Disconnect, RequestStatus, AssignTray };

}  // namespace filament_station::rtos
