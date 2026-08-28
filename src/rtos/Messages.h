/**
 * @file
 * @brief The trivially-copyable value-type messages carried on every
 *        FreeRTOS queue in rtos::RtosContext. See docs/architecture.md,
 *        section "Queues".
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "models/TagReadResult.h"
#include "models/AppState.h"
#include "models/BambuPrinterConfig.h"
#include "models/NetworkSettings.h"
#include "models/PrinterState.h"
#include "models/SpoolmanSettings.h"
#include "models/SpoolmanSpool.h"
#include "models/SpoolmanCatalog.h"
#include "models/TraySpoolCache.h"
#include "rtos/Commands.h"
#include "rtos/Events.h"

namespace filament_station::rtos {

/// @brief Which format a legacy NFC mapping-file entry referred to, during
///        the one-time migration import (see docs/legacy-and-unknown-tags.md).
enum class NfcTagType : std::uint8_t {
  Unknown,
  Spoolman,
  Bambu,
  Legacy,
};

/// @brief One entry read from an obsolete on-card mapping file during the
///        one-time migration import.
// Value type used only while importing obsolete on-card mapping files.
// It is not a runtime tag repository.
struct LegacyNfcMappingEntry {
  std::uint8_t uid[10]{};                 ///< Tag UID this mapping applies to.
  std::uint8_t uidLength = 0;             ///< Number of valid bytes in #uid.
  SpoolId spoolId = 0;                    ///< Spoolman spool id this UID was mapped to.
  models::TagFormat tagFormat = models::TagFormat::Unknown;  ///< Format the mapping file recorded for this tag.
};
constexpr std::size_t kMaximumLegacyNfcMappings = 8;  ///< Maximum number of entries read from one legacy mapping file at a time.

/// @brief The single message type on rtos::RtosContext::appEventQueue --
///        every service task reports its results to tasks::appTask()
///        through this struct, tagged by #type. Only the fields relevant
///        to a given #type are meaningful; the rest are default-valued.
struct AppEvent {
  AppEventType type;                    ///< Which event this is; determines which other fields are meaningful.
  std::uint32_t requestId;              ///< Correlation id echoed back from the triggering command/action.
  std::int32_t value;                   ///< Generic numeric payload, meaning depends on #type.
  std::int32_t scaleOffsetCounts;       ///< HX711 tare offset, for Scale* events.
  float scaleFactorCountsPerGram;       ///< HX711 calibration factor, for Scale* events.
  bool scaleCalibrated;                 ///< Whether the scale is currently calibrated, for Scale* events.
  SpoolId spoolId;                      ///< Resolved Spoolman spool id, for Spoolman*/Nfc* events.
  NfcTagType nfcTagType;                ///< Legacy-migration tag format classification.
  std::uint8_t nfcUid[10];              ///< Raw NFC UID, for Nfc* events.
  std::uint8_t nfcUidLength;            ///< Number of valid bytes in #nfcUid.
  models::TagReadResult tagReadResult;  ///< Full parsed tag result, for NfcTagRead.
  LegacyNfcMappingEntry legacyNfcMappings[kMaximumLegacyNfcMappings];  ///< Entries loaded during legacy mapping-file migration.
  std::uint8_t legacyNfcMappingCount;   ///< Number of valid entries in #legacyNfcMappings.
  models::NetworkSettings networkSettings;    ///< Loaded network configuration, for the corresponding StorageReadCompleted.
  models::SpoolmanSettings spoolmanSettings;  ///< Loaded Spoolman configuration, for the corresponding StorageReadCompleted.
  PrinterId printerId;                  ///< Printer this event concerns, for Bambu* events.
  models::PrinterState printerState{};  ///< Full printer runtime state snapshot, for Bambu* events.
  models::BambuConfigCollection bambuConfigs{};  ///< Loaded printer configuration collection.
  models::TraySpoolCache traySpoolCache{};       ///< Loaded printer/tray -> spool association cache.
  models::SpoolmanSpool spool{};        ///< Loaded spool data, for Spoolman spool responses.
  // Response payload for SpoolmanCommandType::LoadFilament (bambu_temp_min/
  // bambu_temp_max/flow_dynamics_k_factor -- Spoolman *filament* properties, see
  // docs/bambu-protocol.md).
  models::SpoolmanFilament filament{};        ///< Loaded filament data, for SpoolmanCommandType::LoadFilament responses.
  models::SpoolmanWeightUpdate weightUpdate{};  ///< Echoed weight-update request, for SpoolmanWeightUpdated.
  models::TagIdentity tagIdentity{};    ///< Canonical tag identity, for Spoolman tag-lookup events.
  char networkSsid[33];                 ///< Connected/portal SSID, for WiFi events.
  char networkIp[16];                   ///< Assigned/portal IP address, for WiFi events.
  char spoolColorHex[models::SpoolmanSpool::kMaximumColors][9];  ///< Spool colors, for Spoolman spool responses.
  std::uint8_t spoolColorCount;         ///< Number of valid entries in #spoolColorHex.
  // NFC diagnostics and multi-line UI status messages must remain complete.
  // Keep this fixed-size and value based; no pointers or Arduino Strings cross
  // task boundaries.
  char text[192];       ///< Generic text payload (log detail, error message, ...), meaning depends on #type.
  UiAction uiAction;     ///< Embedded UI action, for AppEventType::UiAction.
};

/// @brief The single message type on rtos::RtosContext::uiCommandQueue --
///        AppTask tells UiTask what to render through this struct, tagged
///        by #type.
struct UiCommand {
  UiCommandType type;         ///< Which command this is; determines which other fields are meaningful.
  UiOverlayKind overlayKind;  ///< Which overlay to show, for ShowDialog/ShowProgress.
  std::uint32_t requestId;    ///< Correlation id, echoed back by the eventual UiAction response.
  UiScreenId screenId;        ///< Target screen, for ShowScreen.
  PrinterId printerId;        ///< Target printer, if applicable.
  SpoolId spoolId;            ///< Target/resolved spool, if applicable.
  std::uint8_t amsId;         ///< Target AMS index, if applicable.
  std::uint8_t trayId;        ///< Target tray index, if applicable.
  std::int32_t value;         ///< Generic numeric payload, meaning depends on #type.
  UiNetworkState networkState;  ///< Network status, for UpdateNetworkStatus.
  models::SpoolmanAppState spoolmanAppState =
      models::SpoolmanAppState::SpoolmanUnavailable;  ///< Spoolman readiness, for UpdateSpoolmanState.
  float weightGrams;           ///< Current weight, for UpdateWeight.
  // K-Faktor fuer UpdateTrayDetails, sobald AppTask::resolveTraySpoolDetails()
  // ihn (als Spoolman-*Filament*-Eigenschaft, siehe docs/bambu-protocol.md)
  // geladen hat -- kFactorValid unterscheidet "noch nicht/nicht verfuegbar"
  // von einem tatsaechlichen K-Faktor 0.
  bool kFactorValid = false;   ///< Whether #kFactor has been loaded yet.
  float kFactor = 0.0F;        ///< Loaded flow-dynamics K-factor, only valid if #kFactorValid.
  models::SpoolmanSpool spool{};              ///< Spool data, for the spool picker/detail screens.
  models::SpoolmanWeightUpdate weightUpdate{};  ///< Weight-update payload, for measurement result screens.
  char spoolColorHex[models::SpoolmanSpool::kMaximumColors][9];  ///< Spool colors, for tray/spool display.
  std::uint8_t spoolColorCount;  ///< Number of valid entries in #spoolColorHex.
  char title[48];                ///< Dialog/overlay title text.
  char text[192];                ///< Dialog/overlay body text, or generic status text.
};

/// @brief Command sent from AppTask to tasks::scaleTask().
struct ScaleCommand {
  ScaleCommandType type;             ///< Which command this is.
  std::uint32_t requestId;           ///< Correlation id.
  float referenceWeightGrams;        ///< Known reference weight, for StartCalibration.
  std::int32_t offsetCounts;         ///< Persisted tare offset, for ApplyCalibration.
  float factorCountsPerGram;         ///< Persisted calibration factor, for ApplyCalibration.
  bool calibrated;                   ///< Persisted calibration flag, for ApplyCalibration.
};

/// @brief Command sent from AppTask to tasks::nfcTask().
struct NfcCommand {
  NfcCommandType type;       ///< Which command this is.
  std::uint32_t requestId;   ///< Correlation id.
  std::uint32_t spoolId;     ///< Spool id to write, for WriteSpoolTag.
};

/// @brief Command/acknowledgement exchanged with tasks::powerTask() for the
///        Energiesparen state machine.
struct PowerCommand {
  PowerCommandType type;         ///< Which command/acknowledgement this is.
  std::uint32_t inactiveMs;      ///< Measured input-inactivity duration, for ReportInactivity.
  // Nur fuer type == PowerDownAcknowledged relevant.
  PowerPeripheral source = PowerPeripheral::Scale;  ///< Which peripheral task sent this, only meaningful for PowerDownAcknowledged.
};

/// @brief Which persisted JSON document a StorageCommand refers to.
enum class StorageDocumentType : std::uint8_t {
  Device,
  Network,
  Spoolman,
  Bambu,
  Ui,
  Scale,
  Nfc,
  Diagnostics,
  TraySpoolCache
};
constexpr std::size_t kStorageJsonPayloadCapacity = 768;  ///< Maximum size of the inline JSON payload carried in StorageCommand::json.

/// @brief Command sent from AppTask to tasks::storageTask().
/// @note #path/#documentType are unused for the BambuMaterial* download
///       commands (the temp path is a fixed constant, see StorageTask.cpp) --
///       #json/#jsonLength carry a raw byte chunk for WriteBambuMaterialChunk,
///       or the 64-hex-digit expected SHA-256 for CommitBambuMaterialDownload.
struct StorageCommand {
  StorageCommandType type;           ///< Which operation to perform.
  std::uint32_t requestId;           ///< Correlation id.
  char path[96];                     ///< Absolute SD-card path to operate on.
  StorageDocumentType documentType;  ///< Which document schema #path holds.
  std::uint16_t jsonLength;          ///< Number of valid bytes in #json, for SaveJson/WriteBambuMaterialChunk.
  char json[kStorageJsonPayloadCapacity];  ///< Inline JSON payload to write, for SaveJson; raw chunk bytes or expected SHA-256 for the BambuMaterial* download commands.
};

/// @brief Command sent from AppTask to tasks::networkTask().
struct NetworkCommand {
  NetworkCommandType type;          ///< Which command this is.
  std::uint32_t requestId;          ///< Correlation id.
  models::NetworkSettings settings;  ///< Configuration to apply, for ApplyConfiguration/Reconfigure.
};

/// @brief Command sent from AppTask to tasks::spoolmanTask().
struct SpoolmanCommand {
  SpoolmanCommandType type;   ///< Which operation to perform.
  std::uint32_t requestId;    ///< Correlation id.
  std::uint32_t spoolId;      ///< Target spool id, if applicable.
  // LoadFilament: which filament to fetch (bambu_temp_min/bambu_temp_max/
  // flow_dynamics_k_factor live on the filament, not the spool -- see
  // docs/bambu-protocol.md).
  std::uint32_t filamentId;   ///< Target filament id, for LoadFilament.
  float weightGrams;          ///< New remaining weight, for UpdateWeight.
  models::SpoolmanSettings settings;      ///< Configuration to apply/test, for ApplyConfiguration/HealthCheck.
  models::TagDefinition tagDefinition{};  ///< Parsed tag definition to import, for ImportTagDefinition.
  models::SpoolmanVendor vendor{};        ///< Vendor to find-or-create, for CreateVendor/ImportTagDefinition.
  models::SpoolmanFilament filament{};    ///< Filament to find-or-create, for CreateFilament/ImportTagDefinition.
  models::SpoolmanWeightUpdate weightUpdate{};  ///< Weight fields to write, for UpdateWeight.
  models::TagIdentity tagIdentity{};      ///< Identity to look up/set/clear, for tag-related commands.
  SpoolmanSearchFilter searchFilter = SpoolmanSearchFilter::FilamentName;  ///< Which field #searchText filters by, for search commands.
  bool includeArchived = false;           ///< Whether archived spools/entries are included in a search.
  char searchText[48]{};                  ///< Free-text search query, for search commands.
};

/// @brief Command sent from AppTask to tasks::bambuTask().
struct BambuCommand {
  BambuCommandType type;      ///< Which operation to perform.
  std::uint32_t requestId;    ///< Correlation id.
  PrinterId printerId;        ///< Target printer.
  std::uint8_t amsId;         ///< Target AMS index (wire-encoded, 0-based, or models::kBambuExternalAmsId), for AssignTray.
  std::uint8_t trayId;        ///< Target tray index (wire-encoded, or models::kBambuExternalTrayId), for AssignTray.
  SpoolId spoolId;            ///< Spoolman spool being assigned, for AssignTray (0 clears the slot).
  // Required for Connect/TestConnection: BambuTask has no SD/storage access
  // of its own, so the caller (AppTask) supplies host/serial/access code.
  models::BambuPrinterConfig printerConfig{};  ///< Connection details, for Connect/TestConnection.
  // Required for AssignTray ("Slotdaten schreiben"). Resolving a Spoolman
  // spool to filament type/color happens outside BambuTask (Phase 8.5); the
  // caller supplies the already-resolved values here. `trayType` is the raw
  // Spoolman material name (e.g. "PLA-CF"), empty to clear the slot --
  // BambuTask resolves it to a services::BambuMaterialMapping (tray_info_idx/
  // canonical tray_type/nozzle_temp_min/max) itself; no temperature field
  // here, the wire nozzle_temp_min/max always come from that mapping, never
  // from Spoolman.
  char trayType[16]{};              ///< Material to write, for AssignTray (empty clears the slot).
  char trayColorHex[9]{};           ///< Color to write, for AssignTray.
  // Spoolman filament's flow_dynamics_k_factor, for AssignTray -- fetched by
  // AppTask via the same LoadSpool->LoadFilament chain already used for the
  // Home-screen K-factor display (see AppTask.cpp's SlotAssignmentStage).
  // BambuTask uploads it (extrusion_cali_set/get/sel) only if kFactorValid
  // and the printer has already reported a nozzle type; otherwise AssignTray
  // proceeds exactly as before (fail-closed, see docs/bambu-protocol.md).
  bool kFactorValid = false;        ///< Whether #kFactor is valid, for AssignTray.
  float kFactor = 0.0F;             ///< Flow-dynamics K-factor to upload, for AssignTray, only valid if #kFactorValid.
};

/// @brief Command sent from AppTask to tasks::updateTask().
struct UpdateCommand {
  UpdateCommandType type;    ///< Which operation to perform.
  std::uint32_t requestId;   ///< Correlation id.
};

static_assert(std::is_trivially_copyable_v<AppEvent>);
static_assert(std::is_trivially_copyable_v<UiCommand>);
static_assert(std::is_trivially_copyable_v<UiAction>);
static_assert(std::is_trivially_copyable_v<StorageCommand>);
static_assert(std::is_trivially_copyable_v<NetworkCommand>);
static_assert(std::is_trivially_copyable_v<UpdateCommand>);
static_assert(std::is_trivially_copyable_v<SpoolmanCommand>);
static_assert(std::is_trivially_copyable_v<BambuCommand>);
static_assert(std::is_trivially_copyable_v<PowerCommand>);

}  // namespace filament_station::rtos
