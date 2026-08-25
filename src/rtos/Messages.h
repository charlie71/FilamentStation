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

enum class NfcTagType : std::uint8_t {
  Unknown,
  Spoolman,
  Bambu,
  Legacy,
};

// Value type used only while importing obsolete on-card mapping files.
// It is not a runtime tag repository.
struct LegacyNfcMappingEntry {
  std::uint8_t uid[10]{};
  std::uint8_t uidLength = 0;
  SpoolId spoolId = 0;
  models::TagFormat tagFormat = models::TagFormat::Unknown;
};
constexpr std::size_t kMaximumLegacyNfcMappings = 8;

struct AppEvent {
  AppEventType type;
  std::uint32_t requestId;
  std::int32_t value;
  std::int32_t scaleOffsetCounts;
  float scaleFactorCountsPerGram;
  bool scaleCalibrated;
  SpoolId spoolId;
  NfcTagType nfcTagType;
  std::uint8_t nfcUid[10];
  std::uint8_t nfcUidLength;
  models::TagReadResult tagReadResult;
  LegacyNfcMappingEntry legacyNfcMappings[kMaximumLegacyNfcMappings];
  std::uint8_t legacyNfcMappingCount;
  models::NetworkSettings networkSettings;
  models::SpoolmanSettings spoolmanSettings;
  PrinterId printerId;
  models::PrinterState printerState{};
  models::BambuConfigCollection bambuConfigs{};
  models::TraySpoolCache traySpoolCache{};
  models::SpoolmanSpool spool{};
  // Response payload for SpoolmanCommandType::LoadFilament (bambu_temp_min/
  // bambu_temp_max/flow_dynamics_k_factor -- Spoolman *filament* properties, see
  // docs/bambu-protocol.md).
  models::SpoolmanFilament filament{};
  models::SpoolmanWeightUpdate weightUpdate{};
  models::TagIdentity tagIdentity{};
  char networkSsid[33];
  char networkIp[16];
  char spoolColorHex[models::SpoolmanSpool::kMaximumColors][9];
  std::uint8_t spoolColorCount;
  // NFC diagnostics and multi-line UI status messages must remain complete.
  // Keep this fixed-size and value based; no pointers or Arduino Strings cross
  // task boundaries.
  char text[192];
  UiAction uiAction;
};

struct UiCommand {
  UiCommandType type;
  UiOverlayKind overlayKind;
  std::uint32_t requestId;
  UiScreenId screenId;
  PrinterId printerId;
  SpoolId spoolId;
  std::uint8_t amsId;
  std::uint8_t trayId;
  std::int32_t value;
  UiNetworkState networkState;
  models::SpoolmanAppState spoolmanAppState =
      models::SpoolmanAppState::SpoolmanUnavailable;
  float weightGrams;
  // K-Faktor fuer UpdateTrayDetails, sobald AppTask::resolveTraySpoolDetails()
  // ihn (als Spoolman-*Filament*-Eigenschaft, siehe docs/bambu-protocol.md)
  // geladen hat -- kFactorValid unterscheidet "noch nicht/nicht verfuegbar"
  // von einem tatsaechlichen K-Faktor 0.
  bool kFactorValid = false;
  float kFactor = 0.0F;
  models::SpoolmanSpool spool{};
  models::SpoolmanWeightUpdate weightUpdate{};
  char spoolColorHex[models::SpoolmanSpool::kMaximumColors][9];
  std::uint8_t spoolColorCount;
  char title[48];
  char text[192];
};

struct ScaleCommand {
  ScaleCommandType type;
  std::uint32_t requestId;
  float referenceWeightGrams;
  std::int32_t offsetCounts;
  float factorCountsPerGram;
  bool calibrated;
};
struct NfcCommand { NfcCommandType type; std::uint32_t requestId; std::uint32_t spoolId; };
struct PowerCommand {
  PowerCommandType type;
  std::uint32_t inactiveMs;
};

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
constexpr std::size_t kStorageJsonPayloadCapacity = 768;
struct StorageCommand {
  StorageCommandType type;
  std::uint32_t requestId;
  char path[96];
  StorageDocumentType documentType;
  std::uint16_t jsonLength;
  char json[kStorageJsonPayloadCapacity];
};

struct NetworkCommand {
  NetworkCommandType type;
  std::uint32_t requestId;
  models::NetworkSettings settings;
};
struct SpoolmanCommand {
  SpoolmanCommandType type;
  std::uint32_t requestId;
  std::uint32_t spoolId;
  // LoadFilament: which filament to fetch (bambu_temp_min/bambu_temp_max/
  // flow_dynamics_k_factor live on the filament, not the spool -- see
  // docs/bambu-protocol.md).
  std::uint32_t filamentId;
  float weightGrams;
  models::SpoolmanSettings settings;
  models::TagDefinition tagDefinition{};
  models::SpoolmanVendor vendor{};
  models::SpoolmanFilament filament{};
  models::SpoolmanWeightUpdate weightUpdate{};
  models::TagIdentity tagIdentity{};
  SpoolmanSearchFilter searchFilter = SpoolmanSearchFilter::FilamentName;
  bool includeArchived = false;
  char searchText[48]{};
};
struct BambuCommand {
  BambuCommandType type;
  std::uint32_t requestId;
  PrinterId printerId;
  std::uint8_t amsId;
  std::uint8_t trayId;
  SpoolId spoolId;
  // Required for Connect/TestConnection: BambuTask has no SD/storage access
  // of its own, so the caller (AppTask) supplies host/serial/access code.
  models::BambuPrinterConfig printerConfig{};
  // Required for AssignTray ("Slotdaten schreiben"). Resolving a Spoolman
  // spool to filament type/color happens outside BambuTask (Phase 8.5); the
  // caller supplies the already-resolved values here.
  char trayType[16]{};
  char trayColorHex[9]{};
  std::uint16_t nozzleTempMinC = 0;
  std::uint16_t nozzleTempMaxC = 0;
};

static_assert(std::is_trivially_copyable_v<AppEvent>);
static_assert(std::is_trivially_copyable_v<UiCommand>);
static_assert(std::is_trivially_copyable_v<UiAction>);
static_assert(std::is_trivially_copyable_v<StorageCommand>);
static_assert(std::is_trivially_copyable_v<NetworkCommand>);
static_assert(std::is_trivially_copyable_v<SpoolmanCommand>);
static_assert(std::is_trivially_copyable_v<BambuCommand>);
static_assert(std::is_trivially_copyable_v<PowerCommand>);

}  // namespace filament_station::rtos
