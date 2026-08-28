/**
 * @file
 * @brief UI-facing screen/command/action enums and the per-service command
 *        type enums, plus the value types carried on the corresponding
 *        command queues. See docs/architecture.md, section "Queues".
 */
#pragma once

#include <cstdint>

#include "models/PrinterState.h"

namespace filament_station::rtos {

using PrinterId = models::PrinterId;  ///< Re-exported for rtos-layer code that doesn't otherwise depend on models::.
using SpoolId = std::uint32_t;        ///< Spoolman spool id.

constexpr std::int32_t UI_TAG_CAP_WRITE = 1 << 0;   ///< Bit flag: the tag may be written.
constexpr std::int32_t UI_TAG_CAP_LINK = 1 << 1;    ///< Bit flag: the tag identity may be linked to a spool.
constexpr std::int32_t UI_TAG_CAP_UNLINK = 1 << 2;  ///< Bit flag: an existing link may be removed.
constexpr std::int32_t UI_TAG_CAP_ERASE = 1 << 3;   ///< Bit flag: the tag's native payload may be erased.

/// @brief Every LVGL screen the UI can navigate to. See docs/workflows.md,
///        section "Screens", for what each one shows.
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

/// @brief Every command AppTask can send to UiTask via rtos::UiCommand.
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
  // Energiesparen (TASKS.md Phase 11.2): value traegt die Ziel-Helligkeit
  // (0-255, wie Light_PWM::setBrightness()), gesendet von PowerTask bei
  // jedem Statemachine-Uebergang.
  SetBrightness,
};

/// @brief Network status shown in the UI header/WiFi settings.
enum class UiNetworkState : std::uint8_t {
  Offline,
  Connecting,
  Online,
  PortalActive,
  CredentialsCleared,
};

/// @brief Which overlay dialog UiBridge::showOverlay() should display; see
///        docs/workflows.md.
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
  // Firmware-Update (TASKS.md Phase 13.3).
  UpdateInstallConfirmation,
  UpdateDownload,
};

/// @brief Every user-initiated action UiTask can send to AppTask via
///        rtos::UiAction. See docs/workflows.md for the corresponding
///        end-user workflows and rtos::requiresOnlineSpoolman() for which
///        of these require an online Spoolman connection.
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
  // Bambu material-mapping download (TASKS.md Nachtrag 2026-08-28): triggers
  // tasks::updateTask()'s UpdateCommandType::DownloadBambuMaterials, same
  // settings-screen placement as CheckFirmwareUpdate.
  UpdateBambuMaterials,
};

/// @brief Whether a UiActionType is one of the two public tag-assignment
///        actions (AssignTag/RemoveTagAssignment).
/// @param type Action to check.
/// @return True for AssignTag or RemoveTagAssignment.
constexpr bool isPublicTagAssignmentAction(UiActionType type) {
  return type == UiActionType::AssignTag ||
         type == UiActionType::RemoveTagAssignment;
}

/// @brief Whether a UiActionType requires an online, tag-field-ready
///        Spoolman connection before it may proceed (see
///        docs/workflows.md, section "Spoolman Offline Error Flow").
/// @param type Action to check.
/// @return True if the action is gated on Spoolman being ready.
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

/// @brief One user-initiated action, sent from UiTask to AppTask on
///        rtos::RtosContext::uiCommandQueue's reverse counterpart (the
///        action path uses appEventQueue via AppEventType::UiAction).
struct UiAction {
  UiActionType type = UiActionType::Back;  ///< Which action this is.
  std::uint32_t requestId = 0;  ///< Correlation id echoed back in the eventual response.
  PrinterId printerId = 0;      ///< Target printer, if applicable.
  SpoolId spoolId = 0;          ///< Target Spoolman spool, if applicable.
  std::uint8_t amsId = 0;       ///< Target AMS index (1-based on the UI side), or models::kExternalTraySentinel.
  std::uint8_t trayId = 0;      ///< Target tray index, or models::kExternalTraySentinel.
  std::int32_t value = 0;       ///< Generic numeric payload, meaning depends on #type.
  char text[64]{};              ///< Generic text payload, meaning depends on #type.
};

/// @brief Commands AppTask can send to tasks::scaleTask().
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

/// @brief Commands AppTask can send to tasks::nfcTask().
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

/// @brief Commands AppTask can send to tasks::storageTask().
// CreateBackup wurde bewusst nicht aufgenommen: JsonStorage::atomicSave()
// legt bei jedem Speichern bereits automatisch ein *.bak.json an (Phase 2.4,
// per Wiederherstellungstest verifiziert; recoverAtomicSave() nutzt es beim
// naechsten Boot bei fehlender/beschaedigter Zieldatei) -- ein zusaetzlicher
// manueller Trigger auf demselben Pfad wuerde mit diesem Mechanismus
// kollidieren, siehe TASKS.md 10.2.
enum class StorageCommandType : std::uint8_t {
  LoadJson,
  SaveJson,
  DeleteJson,
  // Bambu material-mapping download activation (TASKS.md Nachtrag
  // 2026-08-28): the JSON payload is too large for a single StorageCommand
  // (see rtos::kStorageJsonPayloadCapacity), so tasks::updateTask() streams
  // it to /config/bambu_materials.tmp.json in 1024-byte chunks (command.json/
  // jsonLength) instead of one inline SaveJson. StorageTask holds the only
  // open temp-file handle; a Begin while one is already open discards the
  // stale one first (same "reject/replace overlapping request" guard used
  // throughout AppTask). See docs/bambu-protocol.md.
  BeginBambuMaterialDownload,
  WriteBambuMaterialChunk,
  // command.json carries the 64-hex-digit expected SHA-256 (lowercase);
  // StorageTask computes the actual hash itself over the written temp file
  // -- the sole authority on activation, independent of anything UpdateTask
  // already checked while streaming.
  CommitBambuMaterialDownload,
  AbortBambuMaterialDownload,
};

/// @brief Commands AppTask can send to tasks::networkTask().
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

/// @brief Commands AppTask can send to tasks::spoolmanTask().
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

/// @brief Which field a SpoolmanCommandType::SearchSpools/SearchFilaments
///        request filters by.
enum class SpoolmanSearchFilter : std::uint8_t {
  FilamentName,
  Material,
  Vendor,
  Id,
};

/// @brief Commands AppTask can send to tasks::bambuTask().
enum class BambuCommandType : std::uint8_t {
  Connect,
  Disconnect,
  TestConnection,
  RequestStatus,
  AssignTray,
  Reset,
  Reconnect,
};

/// @brief Commands AppTask can send to tasks::updateTask().
// Firmware-Update (TASKS.md Phase 13.2/13.3): CheckForUpdate fragt die in
// config/UpdateConfig.h konfigurierte GitHub-Releases-API ab. DownloadUpdate
// loest eine eigene, frische Abfrage plus Download/Flash des ersten
// .bin-Assets aus (kein Zustand aus einem vorherigen CheckForUpdate wird
// wiederverwendet -- vermeidet veraltete Download-URLs).
// DownloadBambuMaterials (TASKS.md Nachtrag 2026-08-28): fetches
// bambu_materials.json/.sha256 from the same GitHub release as the
// firmware, streams the verified bytes to tasks::storageTask() (see
// StorageCommandType::BeginBambuMaterialDownload et al.) instead of
// flashing them -- StorageTask, not UpdateTask, has the final say on
// SHA-256/activation (only StorageTask may touch the SD card, AGENTS.md).
enum class UpdateCommandType : std::uint8_t {
  CheckForUpdate,
  DownloadUpdate,
  DownloadBambuMaterials,
};

/// @brief Commands sent to/from tasks::powerTask() for the Energiesparen
///        state machine (see tasks::PowerState).
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

/// @brief Identifies which hardware task a PowerCommandType::
///        PowerDownAcknowledged came from.
// Energiesparen (TASKS.md Phase 11.6): identifiziert, welcher Hardware-Task
// einen PowerDownAcknowledged geschickt hat, damit PowerTask vor dem echten
// Light-Sleep auf alle drei wartet statt blind eine feste Zeit abzuwarten.
enum class PowerPeripheral : std::uint8_t {
  Scale,
  Nfc,
  Network,
};

}  // namespace filament_station::rtos
