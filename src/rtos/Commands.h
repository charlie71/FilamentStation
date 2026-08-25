#pragma once

#include <cstdint>

#include "models/PrinterState.h"

namespace filament_station::rtos {

using PrinterId = models::PrinterId;
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
  UpdateProgress,
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
    case UiActionType::ConfigureSlotFromStaging:
    case UiActionType::ReapplySlot:
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
  // Energiesparen (TASKS.md Phase 11.3): noch nicht verarbeitet, folgt mit
  // dem HX711-Power-Down.
  PowerDown,
  PowerUp,
};
enum class NfcCommandType : std::uint8_t {
  StartReading,
  StopReading,
  WriteSpoolTag,
  EraseTag,
  // Energiesparen (TASKS.md Phase 11.4): noch nicht verarbeitet, folgt mit
  // dem PN532-Power-Down.
  PowerDown,
  PowerUp,
};
// CreateBackup wurde bewusst nicht aufgenommen: JsonStorage::atomicSave()
// legt bei jedem Speichern bereits automatisch ein *.bak.json an (Phase 2.4,
// per Wiederherstellungstest verifiziert; recoverAtomicSave() nutzt es beim
// naechsten Boot bei fehlender/beschaedigter Zieldatei) -- ein zusaetzlicher
// manueller Trigger auf demselben Pfad wuerde mit diesem Mechanismus
// kollidieren, siehe TASKS.md 10.2.
enum class StorageCommandType : std::uint8_t { LoadJson, SaveJson, DeleteJson };
enum class NetworkCommandType : std::uint8_t {
  ApplyConfiguration,
  RequestStatus,
  Connect,
  Reconfigure,
  StartPortal,
  StopPortal,
  ClearCredentials,
  // Energiesparen (TASKS.md Phase 11.5): noch nicht verarbeitet, folgt mit
  // der WiFi-Abschaltung.
  PowerDown,
  PowerUp,
};
enum class SpoolmanCommandType : std::uint8_t {
  ApplyConfiguration,
  HealthCheck,
  EnsureTagExtraField,
  FindSpoolByTag,
  SetSpoolTag,
  ClearSpoolTag,
  LoadSpool,
  LoadFilament,
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
enum class BambuCommandType : std::uint8_t {
  Connect,
  Disconnect,
  TestConnection,
  RequestStatus,
  AssignTray,
  Reset,
  Reconnect,
};

// Energiesparen (TASKS.md Phase 11.1): ReportInactivity liefert die von
// UiTask ueber LVGL `lv_display_get_inactive_time()` gemessene Zeit seit der
// letzten Eingabe (einzige Stelle, die LVGL beruehrt, siehe "Nur UiTask
// greift auf LVGL zu"). PowerDownAcknowledged ist die Bestaetigung eines
// Hardware-Tasks, dass er auf einen PowerDown-Befehl (Scale-/Nfc-/
// NetworkCommandType) reagiert hat -- wird erst ab Phase 11.3-11.6 tatsaechlich
// gesendet, wenn PowerTask vor dem Light-Sleep auf alle Bestaetigungen wartet.
enum class PowerCommandType : std::uint8_t {
  ReportInactivity,
  PowerDownAcknowledged,
};

}  // namespace filament_station::rtos
