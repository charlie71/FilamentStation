#include "tasks/Tasks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config/BambuConfig.h"
#include "config/NfcConfig.h"
#include "config/ScaleConfig.h"
#include "nfc/TagWritePolicy.h"
#include "models/AppState.h"
#include "models/BambuPrinterConfig.h"
#include "models/PrinterState.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/SpoolmanUrl.h"
#include "services/SpoolmanClient.h"
#include "services/TagAssignmentPolicy.h"
#include "services/TagIdentity.h"
#include "services/Logger.h"

namespace filament_station::tasks {
namespace {

rtos::UiScreenId currentScreen = rtos::UiScreenId::Boot;
rtos::UiScreenId previousScreen = rtos::UiScreenId::Home;
rtos::UiScreenId printerSettingsReturnScreen = rtos::UiScreenId::SettingsHome;
bool uiStartupReady = false;
bool storageStartupReady = false;
bool startupNavigationSent = false;
// Boot progress (real subsystem status, not just the single static
// "SD-Karte und Konfiguration werden geprueft" message the overlay used to
// show for its entire visible lifetime): each slot is filled in as its
// event actually arrives and refreshBootProgress() re-sends the combined
// text, so the boot overlay grows to reflect whichever early subsystems
// (SD, NFC, Waage) finish before Home is shown.
char bootSdStatus[32]{};
char bootNfcStatus[32]{};
char bootScaleStatus[32]{};
rtos::UiOverlayKind pendingOverlay = rtos::UiOverlayKind::None;
bool wifiPortalRequested = false;
bool wifiPortalActive = false;
std::uint32_t wifiPortalRequestId = 0;
models::SpoolmanAppState currentSpoolmanAppState =
    models::SpoolmanAppState::SpoolmanUnavailable;
char currentSpoolmanServerVersion[32] = "-";
// Runtime Bambu printer state (Phase 8.4). Keyed by printerId; entries are
// never discarded on a printer switch (Zustand sichern), so background
// printers keep their last-known data. `activePrinterId` is the printer
// currently shown on Home/Header/AMS ("wechseln"); PrinterState::isActive is
// kept in sync with it. Auto-connecting on switch/boot is not part of Phase
// 8.4/8.6 and remains open.
models::PrinterStateCollection printerCollection{};
// Persisted printer configuration (Phase 8.2 schema, Phase 8.6 CRUD).
// Distinct from printerCollection above: this is what /config/bambu.json
// holds (name/host/serialNumber/accessCode/enabled/default/selected), not
// live connection/AMS status.
models::BambuConfigCollection printerConfigs{};
// Persisted printer->AMS/tray->Spoolman-spool association (Nutzerwunsch
// 2026-08-24, /mappings/printer-slots.json). Replaces an earlier attempt to
// round-trip this through the printer itself via a custom MQTT field, which
// hardware testing showed the printer never persists (see
// docs/bambu-protocol.md). Kept locally instead; validated against live
// material/colorHex on every sync (see resolveTraySpoolCacheSpoolId()) so a
// spool physically swapped outside this app is detected as stale.
models::TraySpoolCache traySpoolCache{};

// Once a tray's Spoolman spool is identified (traySpoolCache above), the
// remaining weight and K-factor are fetched from Spoolman and shown on the
// tray card (Nutzerwunsch 2026-08-24). Small RAM-only cache keyed by
// spoolId, *not* persisted (unlike traySpoolCache): this is live Spoolman
// data (remaining weight changes as filament is used), not an identity
// association -- re-fetched fresh every boot. Sized for a handful of
// simultaneously displayed trays, not persisted associations; on overflow
// the first slot is evicted rather than growing unbounded.
constexpr std::size_t kMaximumTraySpoolDetailsEntries = 8;
constexpr std::uint32_t kTraySpoolDetailsRequestIdBase = 0x54534402U;
// Two sequential fetches per entry: LoadSpool first (for remainingWeightGrams
// and to learn filamentId), then LoadFilament (for K-factor -- a Spoolman
// *filament* property, not a spool property, see docs/bambu-protocol.md).
// Both requests reuse the same slot's requestId since they never overlap in
// time for a given entry.
enum class TraySpoolDetailsStage : std::uint8_t {
  Idle,
  LoadingSpool,
  LoadingFilament,
  Loaded,
};
struct TraySpoolDetailsEntry {
  rtos::SpoolId spoolId = 0;
  TraySpoolDetailsStage stage = TraySpoolDetailsStage::Idle;
  float remainingWeightGrams = 0.0F;
  bool kFactorValid = false;
  float kFactor = 0.0F;
};
std::array<TraySpoolDetailsEntry, kMaximumTraySpoolDetailsEntries>
    traySpoolDetails{};

std::uint32_t pendingBambuSaveRequestId = 0;
bool pendingBambuSaveShowsResult = false;
rtos::PrinterId pendingBambuSaveNotifyPrinterId = models::kInvalidPrinterId;
std::uint32_t pendingPrinterTestRequestId = 0;
constexpr std::uint32_t kScaleLoadRequestId = 0x53430001U;
constexpr std::uint32_t kBambuLoadRequestId = 0x42414D01U;
constexpr std::uint32_t kBambuMappingLoadRequestId = 0x4E464301U;
constexpr std::uint32_t kNfcMappingLoadRequestId = 0x4E464302U;
constexpr std::uint32_t kOpenMappingLoadRequestId = 0x4E464303U;
constexpr std::uint32_t kLegacyMigrationLookupRequestId = 0x4D470101U;
constexpr std::uint32_t kLegacyMigrationLoadSpoolRequestId = 0x4D470102U;
constexpr std::uint32_t kLegacyMigrationSetTagRequestId = 0x4D470103U;
constexpr std::uint32_t kLegacyMigrationDeleteRequestBase = 0x4D470110U;
constexpr std::uint32_t kNetworkLoadRequestId = 0x4E455401U;
constexpr std::uint32_t kSpoolmanLoadRequestId = 0x53504D01U;
constexpr std::uint32_t kTraySpoolCacheLoadRequestId = 0x54534301U;
constexpr std::uint32_t kObsoletePendingWeightDeleteRequestId = 0x57475401U;
constexpr std::uint32_t kObsoletePendingMeasurementsDeleteRequestId =
    0x57475402U;
constexpr std::uint32_t kObsoleteSpoolCacheDeleteRequestId = 0x43414301U;
constexpr std::uint32_t kObsoleteFilamentCacheDeleteRequestId = 0x43414302U;
constexpr std::uint32_t kObsoleteVendorCacheDeleteRequestId = 0x43414303U;
std::uint32_t pendingSpoolmanSaveRequestId = 0;
std::int32_t scaleCounts = 0;
std::int32_t scaleOffsetCounts = 0;
float scaleFactorCountsPerGram = 1.0F;
bool scaleCalibrated = false;
bool scaleStable = false;
bool scaleError = true;
struct QuickWeightState {
  bool pending = false;
  bool hasLastMeasurement = false;
  std::uint32_t requestId = 0;
  rtos::SpoolId spoolId = 0;
  rtos::SpoolId lastMeasurementSpoolId = 0;
  float emptyWeightGrams = 0.0F;
  float pendingGrossWeightGrams = 0.0F;
  float pendingRemainingWeightGrams = 0.0F;
  float lastMeasurementGrams = 0.0F;
  char spoolName[32]{};
};
QuickWeightState quickWeight{};
struct AdvancedWeightState {
  bool pending = false;
  bool committed = false;
  std::int32_t mode = 0;
  rtos::SpoolId spoolId = 0;
  float grossWeightGrams = 0.0F;
  float emptyWeightGrams = 0.0F;
  float initialWeightGrams = 0.0F;
  float remainingWeightGrams = 0.0F;
  char spoolName[32]{};
};
AdvancedWeightState advancedWeight{};
struct WeightUpdateState {
  bool active = false;
  bool advanced = false;
  std::uint32_t requestId = 0;
  models::SpoolmanWeightUpdate update{};
};
WeightUpdateState weightUpdate{};
bool pendingStagingSpoolSelection = false;
std::uint32_t pendingStagingSpoolRequestId = 0;
// After a staging LoadSpool response arrives, a follow-up LoadFilament
// fetch is needed for emptySpoolWeightGrams/K-Faktor (Spoolman *filament*
// properties, see docs/bambu-protocol.md und Nutzerhinweis 2026-08-24) --
// the embedded filament object inside a spool response is not reliable for
// these fields (same issue as bambu_temp_min/bambu_temp_max). The resolved
// spool is cached here across that second request the same way
// pendingSlotAssignment caches trayType/trayColorHex.
struct PendingStagingFilamentLoad {
  bool active = false;
  std::uint32_t requestId = 0;
  models::SpoolmanSpool spool{};
};
PendingStagingFilamentLoad pendingStagingFilamentLoad{};
// AppTask's own authoritative mirror of "is a spool currently staged",
// kept in sync at every point staging content changes (see the three
// UpdateStaging call sites). Lets navigation decisions (SelectStaging,
// Back from StagingActions) skip the empty-staging status screen without
// round-tripping through UiTask's stagingState first.
rtos::SpoolId stagingSpoolId = 0;
enum class PendingTagOperation : std::uint8_t { None, Write, Erase };
PendingTagOperation pendingTagOperation = PendingTagOperation::None;
models::TagReadResult currentTag{};
bool tagPresent = false;
rtos::SpoolId pendingTagSpoolId = 0;
rtos::SpoolId lastUsedTagSpoolId = 0;
enum class TagAssignmentStage : std::uint8_t {
  None,
  SelectingSpool,
  LookingUp,
  AwaitingReassignmentConfirmation,
  ClearingPrevious,
  SettingTarget,
  RollingBackPrevious,
  WritingPayload,
  // Transitional states used only by legacy import/removal code paths. The
  // semantic AssignTag path never persists a local UID mapping.
};
struct PendingTagAssignment {
  TagAssignmentStage stage = TagAssignmentStage::None;
  std::uint32_t requestId = 0;
  rtos::SpoolId spoolId = 0;
  models::TagIdentity identity{};
  rtos::SpoolId previousSpoolId = 0;
  std::array<std::uint8_t, config::kNfcMaxUidLength> uid{};
  std::uint8_t uidLength = 0;
  bool writePayload = false;
  bool tagRemoved = false;
};
PendingTagAssignment pendingTagAssignment{};
bool pendingServerReassignmentConfirmation = false;
models::TagIdentity resolvedTagIdentity{};
rtos::SpoolId resolvedTagSpoolId = 0;
bool pendingUnlinkConfirmation = false;
bool pendingClearStagingConfirmation = false;
enum class TagRemovalStage : std::uint8_t {
  None,
  LookingUp,
  AwaitingConfirmation,
  ClearingServerAssignment,
  // Transitional state used only by legacy code paths outside the semantic
  // RemoveTagAssignment workflow.
  ClearingPayload,
};
struct PendingTagRemoval {
  TagRemovalStage stage = TagRemovalStage::None;
  std::uint32_t requestId = 0;
  rtos::SpoolId spoolId = 0;
  models::TagIdentity identity{};
  std::array<std::uint8_t, config::kNfcMaxUidLength> uid{};
  std::uint8_t uidLength = 0;
  bool clearPayload = false;
  bool tagRemoved = false;
};
PendingTagRemoval pendingTagRemoval{};
struct PendingNativeConsistencyCheck {
  bool active = false;
  std::uint32_t requestId = 0;
  rtos::SpoolId payloadSpoolId = 0;
  models::TagIdentity identity{};
  std::array<std::uint8_t, config::kNfcMaxUidLength> uid{};
  std::uint8_t uidLength = 0;
};
PendingNativeConsistencyCheck pendingNativeConsistency{};
struct PendingTagResolution {
  bool active = false;
  std::uint32_t requestId = 0;
  models::TagIdentity identity{};
  std::array<std::uint8_t, config::kNfcMaxUidLength> uid{};
  std::uint8_t uidLength = 0;
};
PendingTagResolution pendingTagResolution{};

// Phase 8.5 AMS-Zuordnung: assigns the staged Spoolman spule to a printer's
// AMS slot. AppTask does not retain resolved spool data after staging (see
// requestStagingSpool), so the spool is re-loaded here (LoadingSpool) before
// its material/color are sent to the printer via BambuCommand::AssignTray
// (WritingSlot). bambu_temp_min/bambu_temp_max are Spoolman *filament*
// properties (Nutzerhinweis 2026-08-24), fetched via a dedicated
// LoadingFilament step (using the spool's filamentId) rather than trusted
// from the spool response's embedded filament object -- material/colorHex
// are captured in PendingSlotAssignment across that step since they're
// still needed once it completes.
enum class SlotAssignmentStage : std::uint8_t {
  None,
  SelectingSpool,
  LoadingSpool,
  LoadingFilament,
  WritingSlot,
};
struct PendingSlotAssignment {
  SlotAssignmentStage stage = SlotAssignmentStage::None;
  std::uint32_t requestId = 0;
  rtos::PrinterId printerId = 0;
  std::uint8_t amsId = 0;
  std::uint8_t trayId = 0;
  rtos::SpoolId spoolId = 0;
  // Captured from the LoadingSpool response, needed again once
  // LoadingFilament completes.
  char trayType[16]{};
  char trayColorHex[9]{};
  // Set when the resolved filament has no usable bambu_temp_min/
  // bambu_temp_max extra fields (or the LoadFilament fetch itself failed),
  // so the completion dialog can tell the user the printer only received
  // material/color, not a temperature range.
  bool tempFieldsMissing = false;
};
PendingSlotAssignment pendingSlotAssignment{};

struct LegacyMappingFile {
  const char* path = nullptr;
  std::uint32_t loadRequestId = 0;
  bool loadFinished = false;
  bool exists = false;
  bool loadFailed = false;
  bool migrationFailed = false;
  std::array<rtos::LegacyNfcMappingEntry,
             rtos::kMaximumLegacyNfcMappings> mappings{};
  std::uint8_t count = 0;
};

enum class LegacyMigrationStage : std::uint8_t {
  Waiting,
  LookingUp,
  LoadingTarget,
  SettingTarget,
  DeletingFile,
  Complete,
};

std::array<LegacyMappingFile, 3> legacyMappingFiles{{
    {"/mappings/bambu-tags.json", kBambuMappingLoadRequestId},
    {"/mappings/nfc-spools.json", kNfcMappingLoadRequestId},
    {"/mappings/open-tags.json", kOpenMappingLoadRequestId},
}};
LegacyMigrationStage legacyMigrationStage = LegacyMigrationStage::Waiting;
std::uint8_t legacyMigrationFileIndex = 0;
std::uint8_t legacyMigrationEntryIndex = 0;
models::TagIdentity legacyMigrationIdentity{};
std::uint16_t legacyMigrationMigrated = 0;
std::uint16_t legacyMigrationConflicts = 0;

const char* tagTechnologyName(models::TagTechnology technology) {
  switch (technology) {
    case models::TagTechnology::Ntag213: return "NTAG213";
    case models::TagTechnology::Ntag215: return "NTAG215";
    case models::TagTechnology::Ntag216: return "NTAG216";
    case models::TagTechnology::MifareClassic1K: return "MIFARE Classic 1K";
    case models::TagTechnology::MifareClassic4K: return "MIFARE Classic 4K";
    case models::TagTechnology::OtherIso14443A: return "ISO14443A";
    default: return "unbekannt";
  }
}

void formatTagUid(const models::TagReadResult& tag, char* output,
                  std::size_t capacity) {
  if (capacity == 0) return;
  std::size_t used = 0;
  output[0] = '\0';
  for (std::uint8_t index = 0; index < tag.uidLength; ++index) {
    const int written = std::snprintf(output + used, capacity - used,
                                      "%s%02X", index == 0 ? "" : ":",
                                      tag.uid[index]);
    if (written <= 0 || static_cast<std::size_t>(written) >= capacity - used)
      break;
    used += static_cast<std::size_t>(written);
  }
}

rtos::SpoolId mappedNfcSpool(const models::TagReadResult& tag) {
  if (resolvedTagSpoolId != 0 &&
      tag.identity.source == resolvedTagIdentity.source &&
      std::strcmp(tag.identity.value, resolvedTagIdentity.value) == 0)
    return resolvedTagSpoolId;
  return 0;
}

bool assignmentTagMatches(const models::TagReadResult& tag) {
  return pendingTagAssignment.uidLength > 0 &&
         tag.uidLength == pendingTagAssignment.uidLength &&
         std::memcmp(tag.uid, pendingTagAssignment.uid.data(),
                     pendingTagAssignment.uidLength) == 0;
}

bool removalTagMatches(const models::TagReadResult& tag) {
  return pendingTagRemoval.uidLength > 0 &&
         tag.uidLength == pendingTagRemoval.uidLength &&
         std::memcmp(tag.uid, pendingTagRemoval.uid.data(),
                     pendingTagRemoval.uidLength) == 0;
}

std::int32_t stagingTagCapabilities() {
  if (!tagPresent || !currentTag.capabilities.canAssociateByUid) return 0;
  // The authoritative assignment state is resolved online by Spoolman when
  // either semantic action starts. Do not gate an action on legacy SD data.
  return rtos::UI_TAG_CAP_LINK | rtos::UI_TAG_CAP_UNLINK;
}

void applyTagUiState(rtos::UiCommand& command) {
  command.value = stagingTagCapabilities();
  command.spoolId = tagPresent ? mappedNfcSpool(currentTag) : 0;
}

bool sendUiCommand(rtos::RtosContext& ctx, const rtos::UiCommand& command,
                   const char* failureMessage);
bool requestStagingSpool(rtos::RtosContext& ctx, std::uint32_t requestId,
                         rtos::SpoolId spoolId);
bool requestFilamentDetails(rtos::RtosContext& ctx, std::uint32_t requestId,
                            std::uint32_t filamentId);
bool sendPendingSlotAssignTray(rtos::RtosContext& ctx, std::uint32_t requestId,
                               std::uint16_t nozzleTempMinC,
                               std::uint16_t nozzleTempMaxC);
void sendStagingUpdate(rtos::RtosContext& ctx, std::uint32_t requestId,
                       const models::SpoolmanSpool& spool,
                       float emptyWeightGrams, bool kFactorValid,
                       float kFactor);
struct TraySpoolDetailsSnapshot {
  bool loaded = false;
  float remainingWeightGrams = 0.0F;
  bool kFactorValid = false;
  float kFactor = 0.0F;
};
TraySpoolDetailsSnapshot resolveTraySpoolDetails(rtos::RtosContext& ctx,
                                                 rtos::SpoolId spoolId);

void showNativeTagAction(rtos::RtosContext& ctx, std::uint32_t requestId,
                         rtos::SpoolId authoritativeSpoolId,
                         const char* assignmentText) {
  const char* chip =
      currentTag.technology == models::TagTechnology::Ntag213
          ? "NTAG213"
          : (currentTag.technology == models::TagTechnology::Ntag215
                 ? "NTAG215"
                 : "NTAG216");
  rtos::UiCommand navigation{};
  navigation.type = rtos::UiCommandType::ShowScreen;
  navigation.screenId = rtos::UiScreenId::TagActionSelect;
  navigation.requestId = requestId;
  navigation.value = stagingTagCapabilities();
  navigation.spoolId = authoritativeSpoolId;
  std::snprintf(navigation.text, sizeof(navigation.text),
                "%s | %s | %s\n%s", chip,
                currentTag.format == models::TagFormat::FilamentStation
                    ? "FilamentStation"
                    : "leer",
                currentTag.writable ? "beschreibbar" : "schreibgesch\xC3\xBCtzt",
                assignmentText != nullptr ? assignmentText : "Nicht zugeordnet");
  if (sendUiCommand(ctx, navigation,
                    "AppTask: native NFC action screen queue overflow")) {
    previousScreen = currentScreen;
    currentScreen = navigation.screenId;
  }

  // Staging (Phase 9.2): a native tag that already resolves to a known
  // Spoolman spool must be usable, not just show "Tag zuordnen"/"Tag-
  // Zuordnung entfernen" -- load it into Staging so it is immediately ready
  // to weigh/update/assign to an AMS slot. Skipped if a staging load is
  // already in flight (single pending-request slot, see
  // pendingStagingSpoolRequestId) to avoid clobbering an unrelated manual
  // spool selection.
  if (authoritativeSpoolId != 0 && pendingStagingSpoolRequestId == 0) {
    if (requestStagingSpool(ctx, requestId, authoritativeSpoolId))
      pendingStagingSpoolRequestId = requestId;
  }
}

LegacyMappingFile* legacyFileForLoadRequest(std::uint32_t requestId) {
  for (auto& file : legacyMappingFiles)
    if (file.loadRequestId == requestId) return &file;
  return nullptr;
}

bool allLegacyMappingLoadsFinished() {
  for (const auto& file : legacyMappingFiles)
    if (!file.loadFinished) return false;
  return true;
}

bool sendLegacySpoolmanCommand(rtos::RtosContext& ctx,
                               rtos::SpoolmanCommandType type,
                               std::uint32_t requestId,
                               rtos::SpoolId spoolId = 0) {
  rtos::SpoolmanCommand command{};
  command.type = type;
  command.requestId = requestId;
  command.spoolId = spoolId;
  command.tagIdentity = legacyMigrationIdentity;
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

void advanceLegacyMigration(rtos::RtosContext& ctx);

void finishLegacyMigrationEntry(rtos::RtosContext& ctx, bool migrated,
                                const char* reason) {
  auto& file = legacyMappingFiles[legacyMigrationFileIndex];
  const auto& mapping = file.mappings[legacyMigrationEntryIndex];
  if (migrated) {
    ++legacyMigrationMigrated;
    FS_LOGI(services::LogComponent::App,
            "Legacy mapping migrated file=%s entry=%u spool_id=%lu tag=%s",
            file.path, static_cast<unsigned>(legacyMigrationEntryIndex),
            static_cast<unsigned long>(mapping.spoolId),
            legacyMigrationIdentity.value);
  } else {
    file.migrationFailed = true;
    ++legacyMigrationConflicts;
    FS_LOGW(services::LogComponent::App,
            "Legacy mapping retained file=%s entry=%u spool_id=%lu tag=%s reason=%s",
            file.path, static_cast<unsigned>(legacyMigrationEntryIndex),
            static_cast<unsigned long>(mapping.spoolId),
            legacyMigrationIdentity.value,
            reason != nullptr ? reason : "migration_failed");
  }
  ++legacyMigrationEntryIndex;
  legacyMigrationStage = LegacyMigrationStage::Waiting;
  advanceLegacyMigration(ctx);
}

void advanceLegacyMigration(rtos::RtosContext& ctx) {
  while (legacyMigrationFileIndex < legacyMappingFiles.size()) {
    auto& file = legacyMappingFiles[legacyMigrationFileIndex];
    if (!file.exists || file.loadFailed) {
      ++legacyMigrationFileIndex;
      legacyMigrationEntryIndex = 0;
      continue;
    }
    if (legacyMigrationEntryIndex < file.count) {
      const auto& mapping = file.mappings[legacyMigrationEntryIndex];
      if (!services::tagIdentityFromUid(mapping.uid, mapping.uidLength,
                                        legacyMigrationIdentity)) {
        finishLegacyMigrationEntry(ctx, false, "invalid_uid");
        return;
      }
      legacyMigrationStage = LegacyMigrationStage::LookingUp;
      if (!sendLegacySpoolmanCommand(
              ctx, rtos::SpoolmanCommandType::FindSpoolByTag,
              kLegacyMigrationLookupRequestId)) {
        finishLegacyMigrationEntry(ctx, false, "spoolman_queue_full");
      }
      return;
    }
    if (!file.migrationFailed) {
      rtos::StorageCommand remove{};
      remove.type = rtos::StorageCommandType::DeleteJson;
      remove.requestId = kLegacyMigrationDeleteRequestBase +
                         legacyMigrationFileIndex;
      remove.documentType = rtos::StorageDocumentType::Nfc;
      std::snprintf(remove.path, sizeof(remove.path), "%s", file.path);
      legacyMigrationStage = LegacyMigrationStage::DeletingFile;
      if (xQueueSend(ctx.storageCommandQueue, &remove,
                     pdMS_TO_TICKS(1000)) != pdPASS) {
        file.migrationFailed = true;
        legacyMigrationStage = LegacyMigrationStage::Waiting;
        FS_LOGW(services::LogComponent::App,
                "Legacy mapping retained file=%s reason=storage_queue_full",
                file.path);
      } else {
        return;
      }
    }
    ++legacyMigrationFileIndex;
    legacyMigrationEntryIndex = 0;
  }
  legacyMigrationStage = LegacyMigrationStage::Complete;
  FS_LOGI(services::LogComponent::App,
          "Legacy mapping migration complete migrated=%u retained_or_conflicting=%u",
          static_cast<unsigned>(legacyMigrationMigrated),
          static_cast<unsigned>(legacyMigrationConflicts));
}

void tryStartLegacyMigration(rtos::RtosContext& ctx) {
  if (legacyMigrationStage != LegacyMigrationStage::Waiting ||
      !allLegacyMappingLoadsFinished())
    return;
  const EventBits_t required =
      rtos::EVENT_SPOOLMAN_READY | rtos::EVENT_SPOOLMAN_TAG_FIELD_READY;
  if ((xEventGroupGetBits(ctx.systemEventGroup) & required) != required) return;
  legacyMigrationFileIndex = 0;
  legacyMigrationEntryIndex = 0;
  legacyMigrationMigrated = 0;
  legacyMigrationConflicts = 0;
  for (auto& file : legacyMappingFiles) file.migrationFailed = file.loadFailed;
  FS_LOGI(services::LogComponent::App,
          "Legacy mapping migration started files=%u",
          static_cast<unsigned>(legacyMappingFiles.size()));
  advanceLegacyMigration(ctx);
}

float scaleWeightGrams() {
  if (!scaleCalibrated || scaleFactorCountsPerGram == 0.0F) return 0.0F;
  return static_cast<float>(scaleCounts - scaleOffsetCounts) /
         scaleFactorCountsPerGram;
}

struct SpoolmanDraft {
  char name[32] = "Werkstatt";
  char protocol[8] = "http";
  char host[64] = "spoolman.local";
  char port[8] = "7912";
  char basePath[32] = "/api/v1";
  char timeoutMs[8] = "5000";
};

SpoolmanDraft spoolmanDraft{};
struct PrinterDraft {
  rtos::PrinterId id = 1;
  char name[32] = "P1S Werkstatt";
  char host[64] = "192.168.1.50";
  char serial[32] = "01P123456789";
  char accessCode[16] = "12345678";
};
PrinterDraft printerDraft{};

void printerConfigFromDraft(models::BambuPrinterConfig& configOut) {
  configOut.printerId = printerDraft.id;
  std::snprintf(configOut.name, sizeof(configOut.name), "%s", printerDraft.name);
  std::snprintf(configOut.host, sizeof(configOut.host), "%s", printerDraft.host);
  std::snprintf(configOut.serialNumber, sizeof(configOut.serialNumber), "%s",
                printerDraft.serial);
  std::snprintf(configOut.accessCode, sizeof(configOut.accessCode), "%s",
                printerDraft.accessCode);
}

void applyPrinterConfigToDraft(const models::BambuPrinterConfig& config) {
  printerDraft.id = config.printerId;
  std::snprintf(printerDraft.name, sizeof(printerDraft.name), "%s", config.name);
  std::snprintf(printerDraft.host, sizeof(printerDraft.host), "%s", config.host);
  std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "%s",
                config.serialNumber);
  std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "%s",
                config.accessCode);
}

// Loads an existing printer's persisted configuration into the edit draft,
// or resets the draft to a blank template for a not-yet-persisted id (Add).
void loadPrinterDraft(rtos::PrinterId id) {
  const models::BambuPrinterConfig* existing =
      models::findPrinterConfig(printerConfigs, id);
  if (existing != nullptr) {
    applyPrinterConfigToDraft(*existing);
    return;
  }
  printerDraft.id = id;
  std::snprintf(printerDraft.name, sizeof(printerDraft.name), "Neuer Drucker");
  printerDraft.host[0] = '\0';
  printerDraft.serial[0] = '\0';
  printerDraft.accessCode[0] = '\0';
}

// Smallest printerId (1..kMaximumPrinters) not already in printerConfigs, or
// kInvalidPrinterId if the roster is full.
rtos::PrinterId allocatePrinterId() {
  for (rtos::PrinterId candidate = 1; candidate <= models::kMaximumPrinters;
       ++candidate) {
    if (models::findPrinterConfig(printerConfigs, candidate) == nullptr)
      return candidate;
  }
  return models::kInvalidPrinterId;
}

void setDefaultPrinterConfig(rtos::PrinterId id) {
  const std::size_t count = printerConfigs.printerCount < models::kMaximumPrinters
                                ? printerConfigs.printerCount
                                : models::kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index)
    printerConfigs.printers[index].isDefault =
        printerConfigs.printers[index].printerId == id;
  printerConfigs.defaultPrinterId = id;
}

void setSelectedPrinterConfig(rtos::PrinterId id) {
  const std::size_t count = printerConfigs.printerCount < models::kMaximumPrinters
                                ? printerConfigs.printerCount
                                : models::kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index)
    printerConfigs.printers[index].isSelected =
        printerConfigs.printers[index].printerId == id;
  printerConfigs.selectedPrinterId = id;
}

// Finds or inserts a printerConfigs entry. A freshly inserted entry is
// enabled by default and becomes the default printer if it is the first one
// (Spoolman-config validity requires exactly one default once printers
// exist, see models::isValidBambuConfigCollection). Returns nullptr if the
// roster is already at models::kMaximumPrinters.
models::BambuPrinterConfig* upsertPrinterConfig(rtos::PrinterId id) {
  models::BambuPrinterConfig* existing =
      models::findPrinterConfig(printerConfigs, id);
  if (existing != nullptr) return existing;
  if (printerConfigs.printerCount >= models::kMaximumPrinters) return nullptr;
  const bool wasEmpty = printerConfigs.printerCount == 0;
  models::BambuPrinterConfig& entry =
      printerConfigs.printers[printerConfigs.printerCount];
  entry = models::BambuPrinterConfig{};
  entry.printerId = id;
  entry.enabled = true;
  ++printerConfigs.printerCount;
  if (wasEmpty) setDefaultPrinterConfig(id);
  return &entry;
}

// Removes a printer from printerConfigs, keeping the array compact and the
// default-printer invariant intact (promotes the first remaining printer if
// the removed one was the default). A cleared selection is not replaced --
// Phase 8.2 treats "selected" as optional.
bool removePrinterConfig(rtos::PrinterId id) {
  const std::size_t count = printerConfigs.printerCount < models::kMaximumPrinters
                                ? printerConfigs.printerCount
                                : models::kMaximumPrinters;
  std::size_t index = count;
  for (std::size_t candidate = 0; candidate < count; ++candidate) {
    if (printerConfigs.printers[candidate].printerId == id) {
      index = candidate;
      break;
    }
  }
  if (index == count) return false;
  const bool wasDefault = printerConfigs.printers[index].isDefault;
  for (std::size_t shift = index; shift + 1 < count; ++shift)
    printerConfigs.printers[shift] = printerConfigs.printers[shift + 1];
  printerConfigs.printers[count - 1] = models::BambuPrinterConfig{};
  --printerConfigs.printerCount;
  if (printerConfigs.selectedPrinterId == id)
    printerConfigs.selectedPrinterId = models::kInvalidPrinterId;
  if (wasDefault) {
    printerConfigs.defaultPrinterId = models::kInvalidPrinterId;
    if (printerConfigs.printerCount > 0)
      setDefaultPrinterConfig(printerConfigs.printers[0].printerId);
  }
  return true;
}

// Serializes printerConfigs to /config/bambu.json and sends it to
// StorageTask. `showResult` controls whether the eventual write completion
// shows a Success dialog (deliberate edit-save) or stays silent unless it
// fails (quick list toggles: enable/default/active/delete).
bool persistPrinterConfigs(rtos::RtosContext& ctx, std::uint32_t requestId,
                           bool showResult, char* errorOut,
                           std::size_t errorCapacity) {
  JsonDocument document;
  document["schemaVersion"] = 1;
  document["updatedAt"] = "1970-01-01T00:00:00Z";
  document["documentType"] = "bambu";
  document["selectedPrinterId"] = printerConfigs.selectedPrinterId;
  document["defaultPrinterId"] = printerConfigs.defaultPrinterId;
  JsonArray printers = document["printers"].to<JsonArray>();
  const std::size_t count = printerConfigs.printerCount < models::kMaximumPrinters
                                ? printerConfigs.printerCount
                                : models::kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index) {
    const models::BambuPrinterConfig& source = printerConfigs.printers[index];
    JsonObject printer = printers.add<JsonObject>();
    printer["printerId"] = source.printerId;
    printer["name"] = source.name;
    printer["host"] = source.host;
    printer["serialNumber"] = source.serialNumber;
    printer["accessCode"] = source.accessCode;
    printer["enabled"] = source.enabled;
    printer["default"] = source.isDefault;
    printer["selected"] = source.isSelected;
  }
  rtos::StorageCommand storage{};
  storage.type = rtos::StorageCommandType::SaveJson;
  storage.requestId = requestId;
  storage.documentType = rtos::StorageDocumentType::Bambu;
  std::snprintf(storage.path, sizeof(storage.path), "/config/bambu.json");
  const std::size_t length =
      serializeJson(document, storage.json, sizeof(storage.json));
  if (length == 0 || length >= sizeof(storage.json)) {
    std::snprintf(errorOut, errorCapacity, "Konfiguration ist zu gro\xC3\x9F.");
    return false;
  }
  storage.jsonLength = static_cast<std::uint16_t>(length);
  pendingBambuSaveRequestId = requestId;
  pendingBambuSaveShowsResult = showResult;
  if (xQueueSend(ctx.storageCommandQueue, &storage, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    pendingBambuSaveRequestId = 0;
    std::snprintf(errorOut, errorCapacity, "StorageTask ist nicht erreichbar.");
    return false;
  }
  return true;
}

void requestBambuConfiguration(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::LoadJson;
  command.requestId = kBambuLoadRequestId;
  command.documentType = rtos::StorageDocumentType::Bambu;
  std::snprintf(command.path, sizeof(command.path), "/config/bambu.json");
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS)
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=load_bambu_config");
}

void requestTraySpoolCache(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::LoadJson;
  command.requestId = kTraySpoolCacheLoadRequestId;
  command.documentType = rtos::StorageDocumentType::TraySpoolCache;
  std::snprintf(command.path, sizeof(command.path),
               "/mappings/printer-slots.json");
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS)
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=load_tray_spool_cache");
}

// Fire-and-forget: no dialog, no pending-requestId tracking -- a failed save
// just means the association isn't durable yet, retried on the next
// successful assignment. requestId 0 is safe here (nothing in the event
// loop matches a bare 0 without also checking a specific pending state).
void persistTraySpoolCache(rtos::RtosContext& ctx) {
  JsonDocument document;
  document["schemaVersion"] = 1;
  document["updatedAt"] = "1970-01-01T00:00:00Z";
  document["documentType"] = "traySpoolCache";
  JsonArray entries = document["entries"].to<JsonArray>();
  const std::size_t count =
      traySpoolCache.entryCount < models::kMaximumTraySpoolCacheEntries
          ? traySpoolCache.entryCount
          : models::kMaximumTraySpoolCacheEntries;
  for (std::size_t index = 0; index < count; ++index) {
    const models::TraySpoolCacheEntry& source = traySpoolCache.entries[index];
    JsonObject entry = entries.add<JsonObject>();
    entry["printerId"] = source.printerId;
    entry["amsId"] = source.amsId;
    entry["trayId"] = source.trayId;
    entry["spoolId"] = source.spoolId;
    entry["material"] = source.material;
    entry["colorHex"] = source.colorHex;
  }
  rtos::StorageCommand storage{};
  storage.type = rtos::StorageCommandType::SaveJson;
  storage.documentType = rtos::StorageDocumentType::TraySpoolCache;
  std::snprintf(storage.path, sizeof(storage.path),
               "/mappings/printer-slots.json");
  const std::size_t length =
      serializeJson(document, storage.json, sizeof(storage.json));
  if (length == 0 || length >= sizeof(storage.json)) {
    FS_LOGW(services::LogComponent::App,
            "Tray-Spoolman cache too large to persist entries=%u",
            static_cast<unsigned>(count));
    return;
  }
  storage.jsonLength = static_cast<std::uint16_t>(length);
  if (xQueueSend(ctx.storageCommandQueue, &storage, pdMS_TO_TICKS(1000)) !=
      pdPASS)
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=save_tray_spool_cache");
}

// Returns the cached Spoolman spool id for (printerId, amsId, trayId) only
// if the printer's *current* material/colorHex still match what was true at
// assignment time -- otherwise the tray was physically changed outside this
// app since, and the association can no longer be trusted (0 = unknown,
// shown as "?" by the UI).
rtos::SpoolId resolveTraySpoolCacheSpoolId(rtos::PrinterId printerId,
                                           std::uint8_t amsId,
                                           std::uint8_t trayId,
                                           const char* material,
                                           const char* colorHex) {
  const models::TraySpoolCacheEntry* entry = models::findTraySpoolCacheEntry(
      traySpoolCache, printerId, amsId, trayId);
  if (entry == nullptr) return 0;
  if (std::strcmp(entry->material, material) != 0 ||
      std::strcmp(entry->colorHex, colorHex) != 0) {
    return 0;
  }
  return entry->spoolId;
}

// Pushes one printerConfigs entry to the UI's printer list/header rendering
// (UiBridge.cpp printerEntries), which used to be a separate, unfed mock.
// value = 120 + bitmask(enabled=1, isDefault=2, isActive=4); see the
// matching UpdatePrinterList handler in UiBridge.cpp.
void syncPrinterEntryToUi(rtos::RtosContext& ctx, rtos::PrinterId printerId) {
  const models::BambuPrinterConfig* source =
      models::findPrinterConfig(printerConfigs, printerId);
  if (source == nullptr) return;
  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::UpdatePrinterList;
  command.printerId = printerId;
  std::snprintf(command.title, sizeof(command.title), "%s", source->name);
  const bool isActive = printerId == printerCollection.activePrinterId ||
                        source->isSelected;
  // connectionState lives in printerCollection (BambuTask runtime status),
  // not in printerConfigs (persisted host/serial/accessCode) -- without
  // this, the UI's connection badge stayed stuck on its Offline default
  // forever, even once BambuTask actually connected.
  const models::PrinterState* runtime =
      models::findPrinter(printerCollection, printerId);
  const models::PrinterConnectionState connectionState =
      runtime != nullptr ? runtime->connectionState
                         : models::PrinterConnectionState::Offline;
  command.value = 120 + (source->enabled ? 1 : 0) + (source->isDefault ? 2 : 0) +
                  (isActive ? 4 : 0) +
                  (static_cast<std::int32_t>(connectionState) << 3);
  sendUiCommand(ctx, command, "AppTask: printer list sync overflow");
}

// Pushes real AMS/tray occupancy (from printerCollection, populated by
// BambuTask's parsed MQTT reports) to the UI's AMS overview/tray rendering,
// which used to read an unfed static mock (2 fake AMS units always shown,
// fabricated per-tray material/color/weight). One UpdateAmsOverview command
// per present AMS unit (trayId=0xFF marks it as a data sync, not a
// navigation trigger; value = 200 + present(bit0) + occupiedTrayCount<<1),
// plus one UpdateTrayDetails command per tray (value = 300 + occupied,
// title = material, text = colorHex).
void syncAmsToUi(rtos::RtosContext& ctx, rtos::PrinterId printerId) {
  const models::PrinterState* printer =
      models::findPrinter(printerCollection, printerId);
  if (printer == nullptr) return;
  for (std::uint8_t amsIndex = 0; amsIndex < models::kMaximumAmsPerPrinter;
       ++amsIndex) {
    const models::AmsState& ams = printer->amsUnits[amsIndex];
    const std::uint8_t uiAmsId = static_cast<std::uint8_t>(amsIndex + 1);
    if (!ams.present) continue;

    std::uint8_t occupied = 0;
    for (const auto& slot : ams.slots)
      if (slot.state == models::PrinterSlotState::Ready) ++occupied;

    rtos::UiCommand summary{};
    summary.type = rtos::UiCommandType::UpdateAmsOverview;
    summary.printerId = printerId;
    summary.amsId = uiAmsId;
    summary.trayId = 0xFF;
    summary.value = 200 + 1 + (occupied << 1);
    sendUiCommand(ctx, summary, "AppTask: AMS overview sync overflow");

    for (std::uint8_t trayIndex = 0; trayIndex < models::kSlotsPerAms;
         ++trayIndex) {
      const models::PrinterSlotStateData& slot = ams.slots[trayIndex];
      rtos::UiCommand tray{};
      tray.type = rtos::UiCommandType::UpdateTrayDetails;
      tray.printerId = printerId;
      tray.amsId = uiAmsId;
      tray.trayId = trayIndex;
      tray.value =
          300 + (slot.state == models::PrinterSlotState::Ready ? 1 : 0);
      tray.spoolId = resolveTraySpoolCacheSpoolId(
          printerId, amsIndex, trayIndex, slot.material, slot.colorHex);
      std::snprintf(tray.title, sizeof(tray.title), "%s", slot.material);
      std::snprintf(tray.text, sizeof(tray.text), "%s", slot.colorHex);
      if (tray.spoolId != 0) {
        // Restgewicht/K-Faktor aus Spoolman nachladen, sobald die Spule
        // identifiziert ist (Nutzerwunsch 2026-08-24) -- tray.spool.id
        // bleibt sonst 0 (UiBridge liest das als "noch nicht geladen" und
        // zeigt nur das Material). K-Faktor ist eine Spoolman-*Filament*-
        // Eigenschaft (Nutzerhinweis 2026-08-24), daher eigene
        // UiCommand-Felder statt tray.spool (das nur Spulen-Felder traegt).
        const TraySpoolDetailsSnapshot details =
            resolveTraySpoolDetails(ctx, tray.spoolId);
        if (details.loaded) {
          tray.spool.id = tray.spoolId;
          tray.weightGrams = details.remainingWeightGrams;
          tray.kFactorValid = details.kFactorValid;
          tray.kFactor = details.kFactor;
        }
      }
      sendUiCommand(ctx, tray, "AppTask: tray details sync overflow");
    }
  }

  rtos::UiCommand external{};
  external.type = rtos::UiCommandType::UpdateTrayDetails;
  external.printerId = printerId;
  external.amsId = 0xFF;
  external.trayId = 0xFF;
  external.value = 300 + (printer->externalSlot.state ==
                                  models::PrinterSlotState::Ready
                              ? 1
                              : 0);
  external.spoolId = resolveTraySpoolCacheSpoolId(
      printerId, models::kExternalTraySentinel,
      models::kExternalTraySentinel, printer->externalSlot.material,
      printer->externalSlot.colorHex);
  std::snprintf(external.title, sizeof(external.title), "%s",
                printer->externalSlot.material);
  std::snprintf(external.text, sizeof(external.text), "%s",
                printer->externalSlot.colorHex);
  if (external.spoolId != 0) {
    const TraySpoolDetailsSnapshot details =
        resolveTraySpoolDetails(ctx, external.spoolId);
    if (details.loaded) {
      external.spool.id = external.spoolId;
      external.weightGrams = details.remainingWeightGrams;
      external.kFactorValid = details.kFactorValid;
      external.kFactor = details.kFactor;
    }
  }
  sendUiCommand(ctx, external, "AppTask: external tray sync overflow");
}

void syncAllPrinterEntriesToUi(rtos::RtosContext& ctx) {
  const std::size_t count = printerConfigs.printerCount < models::kMaximumPrinters
                                ? printerConfigs.printerCount
                                : models::kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index)
    syncPrinterEntryToUi(ctx, printerConfigs.printers[index].printerId);
}

char* printerField(std::int32_t field) {
  switch (field) {
    case 1: return printerDraft.name;
    case 2: return printerDraft.host;
    case 3: return printerDraft.serial;
    case 4: return printerDraft.accessCode;
    default: return nullptr;
  }
}

std::size_t printerFieldCapacity(std::int32_t field) {
  switch (field) {
    case 1: return sizeof(printerDraft.name);
    case 2: return sizeof(printerDraft.host);
    case 3: return sizeof(printerDraft.serial);
    case 4: return sizeof(printerDraft.accessCode);
    default: return 0;
  }
}

const char* validatePrinterDraft() {
  if (printerDraft.name[0] == '\0') return "Fehler: Anzeigename fehlt";
  if (printerDraft.host[0] == '\0' || std::strchr(printerDraft.host, ' ') != nullptr)
    return "Fehler: Host/IP ungültig";
  if (printerDraft.serial[0] == '\0') return "Fehler: Seriennummer fehlt";
  if (std::strlen(printerDraft.accessCode) != 8)
    return "Fehler: LAN-Code muss 8 Zeichen haben";
  return nullptr;
}

char* spoolmanField(std::int32_t field) {
  switch (field) {
    case 1:
      return spoolmanDraft.name;
    case 2:
      return spoolmanDraft.protocol;
    case 3:
      return spoolmanDraft.host;
    case 4:
      return spoolmanDraft.port;
    case 5:
      return spoolmanDraft.basePath;
    case 6:
      return spoolmanDraft.timeoutMs;
    default:
      return nullptr;
  }
}

std::size_t spoolmanFieldCapacity(std::int32_t field) {
  switch (field) {
    case 1:
      return sizeof(spoolmanDraft.name);
    case 2:
      return sizeof(spoolmanDraft.protocol);
    case 3:
      return sizeof(spoolmanDraft.host);
    case 4:
      return sizeof(spoolmanDraft.port);
    case 5:
      return sizeof(spoolmanDraft.basePath);
    case 6:
      return sizeof(spoolmanDraft.timeoutMs);
    default:
      return 0;
  }
}

bool validNumber(const char* text, long minimum, long maximum) {
  char* end = nullptr;
  const long value = std::strtol(text, &end, 10);
  return text[0] != '\0' && end != nullptr && *end == '\0' &&
         value >= minimum && value <= maximum;
}

const char* validateSpoolmanDraft() {
  if (spoolmanDraft.name[0] == '\0') {
    return "Fehler: Verbindungsname fehlt";
  }
  if (std::strcmp(spoolmanDraft.protocol, "http") != 0 &&
      std::strcmp(spoolmanDraft.protocol, "https") != 0) {
    return "Fehler: Protokoll ungültig";
  }
  if (spoolmanDraft.host[0] == '\0' ||
      std::strchr(spoolmanDraft.host, ' ') != nullptr) {
    return "Fehler: Host/IP ungültig";
  }
  if (!validNumber(spoolmanDraft.port, 1, 65535)) {
    return "Fehler: Port muss 1..65535 sein";
  }
  if (spoolmanDraft.basePath[0] != '/') {
    return "Fehler: Basispfad muss mit / beginnen";
  }
  if (!validNumber(spoolmanDraft.timeoutMs, 1000, 60000)) {
    return "Fehler: Timeout muss 1000..60000 ms sein";
  }
  return nullptr;
}

bool sendUiCommand(rtos::RtosContext& ctx, const rtos::UiCommand& command,
                   const char* failureMessage);

bool spoolmanSettingsFromDraft(models::SpoolmanSettings& settings) {
  services::SpoolmanUrlParts parts{};
  std::snprintf(parts.protocol, sizeof(parts.protocol), "%s", spoolmanDraft.protocol);
  std::snprintf(parts.host, sizeof(parts.host), "%s", spoolmanDraft.host);
  std::snprintf(parts.port, sizeof(parts.port), "%s", spoolmanDraft.port);
  std::snprintf(parts.basePath, sizeof(parts.basePath), "%s", spoolmanDraft.basePath);
  settings.enabled = true;
  std::snprintf(settings.name, sizeof(settings.name), "%s", spoolmanDraft.name);
  settings.timeoutMs = static_cast<std::uint32_t>(std::strtoul(spoolmanDraft.timeoutMs, nullptr, 10));
  return services::buildNormalizedSpoolmanUrl(parts, settings.serverUrl,
                                               sizeof(settings.serverUrl));
}

void applySpoolmanSettingsToDraft(const models::SpoolmanSettings& settings) {
  services::SpoolmanUrlParts parts{};
  if (settings.serverUrl[0] != '\0' &&
      services::parseNormalizedSpoolmanUrl(settings.serverUrl, parts)) {
    std::snprintf(spoolmanDraft.protocol, sizeof(spoolmanDraft.protocol), "%s", parts.protocol);
    std::snprintf(spoolmanDraft.host, sizeof(spoolmanDraft.host), "%s", parts.host);
    std::snprintf(spoolmanDraft.port, sizeof(spoolmanDraft.port), "%s", parts.port);
    std::snprintf(spoolmanDraft.basePath, sizeof(spoolmanDraft.basePath), "%s", parts.basePath);
  }
  std::snprintf(spoolmanDraft.name, sizeof(spoolmanDraft.name), "%s", settings.name);
  std::snprintf(spoolmanDraft.timeoutMs, sizeof(spoolmanDraft.timeoutMs), "%lu",
                static_cast<unsigned long>(settings.timeoutMs));
}

// Pushes the current printerDraft field values to the editor UI. Without
// this, the LVGL editor keeps showing whatever placeholder text it last had
// (its own local mock draft) instead of the real, just-loaded config, even
// though AppTask's own printerDraft (and therefore Save/Test) already has
// the correct values -- confusing and untestable against real hardware.
// Mirrors EditPrinterField's existing "value = 20 + field" convention, so
// this reuses the already-working live-edit echo path for the initial load.
void sendPrinterDraftToUi(rtos::RtosContext& ctx) {
  for (std::int32_t field = 1; field <= 4; ++field) {
    rtos::UiCommand command{};
    command.type = rtos::UiCommandType::UpdateSettings;
    command.value = 20 + field;
    command.printerId = printerDraft.id;
    std::snprintf(command.text, sizeof(command.text), "%s", printerField(field));
    sendUiCommand(ctx, command, "AppTask: printer settings UI queue overflow");
  }
}

void sendSpoolmanDraftToUi(rtos::RtosContext& ctx) {
  for (std::int32_t field = 1; field <= 6; ++field) {
    rtos::UiCommand command{};
    command.type = rtos::UiCommandType::UpdateSettings;
    command.value = field;
    std::snprintf(command.text, sizeof(command.text), "%s", spoolmanField(field));
    sendUiCommand(ctx, command, "AppTask: Spoolman settings UI queue overflow");
  }
}

void requestSpoolmanConfiguration(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::LoadJson;
  command.requestId = kSpoolmanLoadRequestId;
  command.documentType = rtos::StorageDocumentType::Spoolman;
  std::snprintf(command.path, sizeof(command.path), "/config/spoolman.json");
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=load_spoolman_config");
}

bool requestSpoolSearch(rtos::RtosContext& ctx, std::uint32_t requestId,
                        const char* searchText = "",
                        rtos::SpoolmanSearchFilter filter =
                            rtos::SpoolmanSearchFilter::FilamentName) {
  models::SpoolmanSettings settings{};
  if (validateSpoolmanDraft() != nullptr ||
      !spoolmanSettingsFromDraft(settings))
    return false;
  rtos::UiCommand reset{};
  reset.type = rtos::UiCommandType::UpdateSpoolPicker;
  reset.requestId = requestId;
  reset.value = -2;
  std::snprintf(reset.text, sizeof(reset.text), "Spulen werden geladen ...");
  sendUiCommand(ctx, reset, "AppTask: picker reset queue overflow");
  rtos::SpoolmanCommand command{};
  command.type = filter == rtos::SpoolmanSearchFilter::Id
                     ? rtos::SpoolmanCommandType::LoadSpool
                     : rtos::SpoolmanCommandType::SearchSpools;
  command.requestId = requestId;
  command.settings = settings;
  command.searchFilter = filter;
  std::snprintf(command.searchText, sizeof(command.searchText), "%s",
                searchText != nullptr ? searchText : "");
  if (filter == rtos::SpoolmanSearchFilter::Id) {
    char* end = nullptr;
    const unsigned long id = std::strtoul(command.searchText, &end, 10);
    if (command.searchText[0] == '\0' || end == nullptr || *end != '\0' ||
        id == 0 || id > UINT32_MAX)
      return false;
    command.spoolId = static_cast<rtos::SpoolId>(id);
  }
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

bool requestStagingSpool(rtos::RtosContext& ctx, std::uint32_t requestId,
                         rtos::SpoolId spoolId) {
  models::SpoolmanSettings settings{};
  if (spoolId == 0 || !spoolmanSettingsFromDraft(settings)) return false;
  rtos::SpoolmanCommand command{};
  command.type = rtos::SpoolmanCommandType::LoadSpool;
  command.requestId = requestId;
  command.spoolId = spoolId;
  command.settings = settings;
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

// bambu_temp_min/bambu_temp_max/flow_dynamics_k_factor are Spoolman *filament*
// properties (Nutzerhinweis 2026-08-24) -- fetched via their own request
// instead of trusting a spool response's embedded filament object.
bool requestFilamentDetails(rtos::RtosContext& ctx, std::uint32_t requestId,
                            std::uint32_t filamentId) {
  models::SpoolmanSettings settings{};
  if (filamentId == 0 || !spoolmanSettingsFromDraft(settings)) return false;
  rtos::SpoolmanCommand command{};
  command.type = rtos::SpoolmanCommandType::LoadFilament;
  command.requestId = requestId;
  command.filamentId = filamentId;
  command.settings = settings;
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

TraySpoolDetailsEntry* findOrCreateTraySpoolDetails(rtos::SpoolId spoolId) {
  for (auto& entry : traySpoolDetails) {
    if (entry.spoolId == spoolId) return &entry;
  }
  for (auto& entry : traySpoolDetails) {
    if (entry.spoolId == 0) {
      entry.spoolId = spoolId;
      return &entry;
    }
  }
  // Cache full: evict the first slot. Realistic usage (a handful of
  // simultaneously displayed, distinct spools) never gets close to this.
  traySpoolDetails[0] = TraySpoolDetailsEntry{};
  traySpoolDetails[0].spoolId = spoolId;
  return &traySpoolDetails[0];
}

// Returns the cached remaining weight/K-factor for `spoolId` if already
// fetched from Spoolman; otherwise (re-)starts the LoadSpool->LoadFilament
// fetch chain (unless one is already in flight) and returns `loaded ==
// false` for now -- the next sync after the response arrives (see the
// SpoolmanResponse/SpoolmanError handlers further down) will have it. Never
// re-fetches once loaded: this is a display-only snapshot, not kept fresh
// against later weight changes (Nutzerwunsch 2026-08-24).
TraySpoolDetailsSnapshot resolveTraySpoolDetails(rtos::RtosContext& ctx,
                                                 rtos::SpoolId spoolId) {
  TraySpoolDetailsSnapshot snapshot{};
  if (spoolId == 0) return snapshot;
  TraySpoolDetailsEntry* entry = findOrCreateTraySpoolDetails(spoolId);
  if (entry->stage == TraySpoolDetailsStage::Loaded) {
    snapshot.loaded = true;
    snapshot.remainingWeightGrams = entry->remainingWeightGrams;
    snapshot.kFactorValid = entry->kFactorValid;
    snapshot.kFactor = entry->kFactor;
    return snapshot;
  }
  if (entry->stage != TraySpoolDetailsStage::Idle) return snapshot;
  const std::size_t index =
      static_cast<std::size_t>(entry - traySpoolDetails.data());
  if (requestStagingSpool(
          ctx, kTraySpoolDetailsRequestIdBase + static_cast<std::uint32_t>(index),
          spoolId)) {
    entry->stage = TraySpoolDetailsStage::LoadingSpool;
  }
  return snapshot;
}

bool sendUiCommand(rtos::RtosContext& ctx, const rtos::UiCommand& command,
                   const char* failureMessage) {
  if (xQueueSend(ctx.uiCommandQueue, &command, pdMS_TO_TICKS(1000)) == pdPASS) {
    return true;
  }
  (void)failureMessage;
  FS_LOGW(services::LogComponent::App,
          "Command enqueue failed queue=ui command=%u request_id=%lu",
          static_cast<unsigned>(command.type),
          static_cast<unsigned long>(command.requestId));
  return false;
}

bool sendScaleCommand(rtos::RtosContext& ctx,
                      const rtos::ScaleCommand& command) {
  if (xQueueSend(ctx.scaleCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=scale command=%u",
            static_cast<unsigned>(command.type));
    return false;
  }
  // Startup race (real crash seen on hardware): AppTask is created before
  // ScaleTask in RtosContext::createServiceTasks() and can be scheduled on
  // the other core immediately, so it is possible for AppTask to process
  // SdMounted -> requestScaleConfiguration -> this call before
  // xTaskCreatePinnedToCore() for ScaleTask has returned and populated
  // ctx.scaleTask. xTaskNotifyGive() on a null handle hits FreeRTOS's
  // configASSERT(xTaskToNotify) and aborts. Safe to just skip the notify
  // in that case: ScaleTask's loop wakes on a bounded timeout
  // (kHx711ReadyTimeoutMs) regardless and drains the whole queue every
  // time, so the command already sitting in scaleCommandQueue is still
  // picked up shortly, just not instantly.
  if (ctx.scaleTask != nullptr) xTaskNotifyGive(ctx.scaleTask);
  return true;
}

bool sendNetworkCommand(rtos::RtosContext& ctx,
                        const rtos::NetworkCommand& command) {
  if (xQueueSend(ctx.networkCommandQueue, &command, pdMS_TO_TICKS(1000)) ==
      pdPASS) {
    return true;
  }
  FS_LOGW(services::LogComponent::App,
          "Command enqueue failed queue=network command=%u",
          static_cast<unsigned>(command.type));
  return false;
}

bool sendBambuCommand(rtos::RtosContext& ctx,
                      const rtos::BambuCommand& command) {
  if (xQueueSend(ctx.bambuCommandQueue, &command, pdMS_TO_TICKS(1000)) ==
      pdPASS) {
    return true;
  }
  FS_LOGW(services::LogComponent::App,
          "Command enqueue failed queue=bambu command=%u",
          static_cast<unsigned>(command.type));
  return false;
}

// Builds and sends the AssignTray command from pendingSlotAssignment's
// captured material/colorHex (set when its LoadingSpool step completed),
// plus whatever nozzle temperature range the caller resolved (0/0 if
// bambu_temp_min/bambu_temp_max are missing/invalid/unreachable -- no
// temperature is ever invented, see the LoadingFilament response handler).
bool sendPendingSlotAssignTray(rtos::RtosContext& ctx, std::uint32_t requestId,
                               std::uint16_t nozzleTempMinC,
                               std::uint16_t nozzleTempMaxC) {
  rtos::BambuCommand assignTray{};
  assignTray.type = rtos::BambuCommandType::AssignTray;
  assignTray.requestId = requestId;
  assignTray.printerId = pendingSlotAssignment.printerId;
  // pendingSlotAssignment.amsId is the UI-side 1-based AMS number
  // (validated as 1..kMaximumAmsPerPrinter at the ConfigureSlotFromStaging/
  // ReapplySlot entry point); the wire protocol counts AMS units 0-based
  // (see the matching conversion/comment at ResetSlot/UntagSlot).
  assignTray.amsId = static_cast<std::uint8_t>(pendingSlotAssignment.amsId - 1U);
  assignTray.trayId = pendingSlotAssignment.trayId;
  assignTray.spoolId = pendingSlotAssignment.spoolId;
  std::snprintf(assignTray.trayType, sizeof(assignTray.trayType), "%s",
               pendingSlotAssignment.trayType);
  std::snprintf(assignTray.trayColorHex, sizeof(assignTray.trayColorHex), "%s",
               pendingSlotAssignment.trayColorHex);
  assignTray.nozzleTempMinC = nozzleTempMinC;
  assignTray.nozzleTempMaxC = nozzleTempMaxC;
  FS_LOGD(services::LogComponent::App,
          "Sending AssignTray request_id=%lu spool_id=%lu "
          "nozzle_temp_min=%u nozzle_temp_max=%u",
          static_cast<unsigned long>(requestId),
          static_cast<unsigned long>(assignTray.spoolId), nozzleTempMinC,
          nozzleTempMaxC);
  return sendBambuCommand(ctx, assignTray);
}

// Builds and sends the UpdateStaging UiCommand plus the "ins Staging
// geladen" toast once the staged spool's data is complete -- either after
// the LoadFilament follow-up finished (emptyWeightGrams/kFactor from the
// filament, see PendingStagingFilamentLoad) or, on a failed/skipped
// follow-up, immediately with just the spool-level data (graceful
// degradation, same pattern as sendPendingSlotAssignTray()).
void sendStagingUpdate(rtos::RtosContext& ctx, std::uint32_t requestId,
                       const models::SpoolmanSpool& spool,
                       float emptyWeightGrams, bool kFactorValid,
                       float kFactor) {
  stagingSpoolId = spool.id;
  rtos::UiCommand staging{};
  staging.type = rtos::UiCommandType::UpdateStaging;
  staging.requestId = requestId;
  staging.spoolId = spool.id;
  staging.spool = spool;
  staging.spool.emptyWeightGrams = emptyWeightGrams;
  staging.kFactorValid = kFactorValid;
  staging.kFactor = kFactor;
  FS_LOGD(services::LogComponent::App,
          "Sending UpdateStaging request_id=%lu spool_id=%lu "
          "empty_weight=%.1f kfactor_valid=%d kfactor=%.3f",
          static_cast<unsigned long>(requestId),
          static_cast<unsigned long>(spool.id),
          static_cast<double>(emptyWeightGrams), kFactorValid,
          static_cast<double>(kFactor));
  sendUiCommand(ctx, staging, "AppTask: staging selection overflow");
  rtos::UiCommand toast{};
  toast.type = rtos::UiCommandType::ShowToast;
  toast.requestId = requestId;
  std::snprintf(toast.text, sizeof(toast.text),
               "Spule #%lu ins Staging geladen",
               static_cast<unsigned long>(spool.id));
  sendUiCommand(ctx, toast, "AppTask: staging toast overflow");
}

// Auto-connect every enabled printer so real status/AMS data is available
// without requiring an explicit user action first (was an open gap through
// Phase 8.6: bambu.json was loaded but never actually handed to BambuTask).
// Called once bambu.json finishes loading and again once WiFi comes up,
// since either can happen first at boot; BambuCommand::Connect is
// idempotent (BambuTask::handleConnect), so calling it twice for the same
// printer is harmless.
//
// Requires EVENT_WIFI_CONNECTED: BambuTask's WiFiClientSecure::connect()
// opens a raw socket via lwIP, which asserts ("Invalid mbox") if the TCP/IP
// task is not yet running -- a real crash seen when this fired straight off
// SdMounted, before the network stack was up. Reaching EVENT_WIFI_CONNECTED
// requires a completed DHCP lease, which guarantees lwIP's tcpip task is
// already running, so gating on it is a safe, sufficient readiness check.
void connectAllEnabledPrinters(rtos::RtosContext& ctx) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_WIFI_CONNECTED) ==
      0) {
    return;
  }
  const std::size_t printerCount =
      printerConfigs.printerCount < models::kMaximumPrinters
          ? printerConfigs.printerCount
          : models::kMaximumPrinters;
  for (std::size_t index = 0; index < printerCount; ++index) {
    const models::BambuPrinterConfig& source = printerConfigs.printers[index];
    if (!source.enabled) continue;
    rtos::BambuCommand connect{};
    connect.type = rtos::BambuCommandType::Connect;
    connect.printerId = source.printerId;
    connect.printerConfig = source;
    sendBambuCommand(ctx, connect);
  }
}

// Spoolman auto-connect fix: SpoolmanTask::healthCheck() bails out
// immediately with "Keine WLAN-Verbindung" if it runs before
// EVENT_WIFI_CONNECTED is set -- a real, observed race at boot (Storage
// can finish loading Spoolman settings, triggering the initial
// ApplyConfiguration-driven health check, before WiFi finishes
// connecting). Without a retry here that first failed attempt would be
// the only one ever made, leaving every tag-assignment action disabled
// for the rest of the session even though Spoolman is genuinely
// reachable. HealthCheck is naturally idempotent (same as
// connectAllEnabledPrinters above), so retrying on every later
// WifiGotIp is safe; the EVENT_SPOOLMAN_READY guard stops retrying once
// a check has actually succeeded.
void retrySpoolmanHealthCheckIfNeeded(rtos::RtosContext& ctx) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_SPOOLMAN_READY) !=
      0) {
    return;
  }
  models::SpoolmanSettings settings{};
  if (!spoolmanSettingsFromDraft(settings) || settings.serverUrl[0] == '\0')
    return;
  rtos::SpoolmanCommand spoolman{};
  spoolman.type = rtos::SpoolmanCommandType::HealthCheck;
  spoolman.requestId = kSpoolmanLoadRequestId;
  spoolman.settings = settings;
  xQueueSend(ctx.spoolmanCommandQueue, &spoolman, pdMS_TO_TICKS(1000));
}

// Looks up printerId in printerCollection, inserting a fresh entry if this
// is the first time this printer has been seen. Never removes entries, so
// callers rely on this to keep background printers' state across a switch.
models::PrinterState& printerEntry(rtos::PrinterId printerId) {
  models::PrinterState* existing =
      models::findPrinter(printerCollection, printerId);
  if (existing != nullptr) return *existing;

  if (printerCollection.printerCount >= models::kMaximumPrinters) {
    // Defensive fallback; not expected with the current kMaximumPrinters
    // cap of 4 concurrent printers (Phase 8.1/8.3).
    FS_LOGW(services::LogComponent::App,
            "Printer collection full; reusing last slot printer_id=%u",
            static_cast<unsigned>(printerId));
    models::PrinterState& reused =
        printerCollection.printers[models::kMaximumPrinters - 1];
    reused = models::PrinterState{};
    reused.printerId = printerId;
    return reused;
  }

  models::PrinterState& entry =
      printerCollection.printers[printerCollection.printerCount];
  entry = models::PrinterState{};
  entry.printerId = printerId;
  ++printerCollection.printerCount;
  return entry;
}

void sendScaleUiState(rtos::RtosContext& ctx, std::uint32_t requestId = 0) {
  static TickType_t lastUpdateTick = 0;
  static std::int32_t lastFlags = -1;
  const std::int32_t flags = (scaleStable ? 1 : 0) |
                             (scaleCalibrated ? 2 : 0) |
                             (scaleError ? 4 : 0);
  const TickType_t now = xTaskGetTickCount();
  const TickType_t minimumInterval =
      pdMS_TO_TICKS(config::kScaleUiUpdateIntervalMs);
  const bool stateChanged = flags != lastFlags;
  if (requestId == 0 && !stateChanged &&
      static_cast<TickType_t>(now - lastUpdateTick) < minimumInterval) {
    return;
  }

  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::UpdateWeight;
  command.requestId = requestId;
  command.weightGrams = scaleWeightGrams();
  command.value = flags;
  std::snprintf(command.text, sizeof(command.text), "%s",
                scaleError ? "Waagenfehler"
                           : (!scaleCalibrated ? "nicht kalibriert"
                                               : (scaleStable ? "stabil" : "instabil")));
  if (sendUiCommand(ctx, command, "AppTask: weight UI queue overflow")) {
    lastUpdateTick = now;
    lastFlags = flags;
  }
}

bool requestScaleConfiguration(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::LoadJson;
  command.requestId = kScaleLoadRequestId;
  command.documentType = rtos::StorageDocumentType::Scale;
  std::snprintf(command.path, sizeof(command.path), "/config/scale.json");
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=load_scale_config");
    return false;
  }
  return true;
}

bool sendWeightUpdate(rtos::RtosContext& ctx, std::uint32_t requestId,
                      const models::SpoolmanWeightUpdate& update) {
  rtos::SpoolmanCommand command{};
  command.type = rtos::SpoolmanCommandType::UpdateWeight;
  command.requestId = requestId;
  command.weightUpdate = update;
  if (!spoolmanSettingsFromDraft(command.settings)) {
    FS_LOGE(services::LogComponent::App,
            "Weight update rejected reason=invalid_spoolman_configuration");
    return false;
  }
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

void deleteObsoleteStorageFile(rtos::RtosContext& ctx,
                               std::uint32_t requestId,
                               const char* path) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::DeleteJson;
  command.requestId = requestId;
  command.documentType = rtos::StorageDocumentType::Diagnostics;
  std::snprintf(command.path, sizeof(command.path), "%s", path);
  if (xQueueSend(ctx.storageCommandQueue, &command,
                 pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=delete_obsolete_file path=%s",
            path);
}

bool requestNetworkConfiguration(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::LoadJson;
  command.requestId = kNetworkLoadRequestId;
  command.documentType = rtos::StorageDocumentType::Network;
  std::snprintf(command.path, sizeof(command.path), "/config/network.json");
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=load_network_config");
    return false;
  }
  return true;
}

bool persistScaleConfiguration(rtos::RtosContext& ctx,
                               const rtos::AppEvent& event) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::SaveJson;
  command.requestId = event.requestId;
  command.documentType = rtos::StorageDocumentType::Scale;
  std::snprintf(command.path, sizeof(command.path), "/config/scale.json");
  const int length = std::snprintf(
      command.json, sizeof(command.json),
      "{\"schemaVersion\":1,\"updatedAt\":\"1970-01-01T00:00:00Z\","
      "\"documentType\":\"scale\",\"calibrated\":%s,"
      "\"tareOffsetCounts\":%ld,\"factorCountsPerGram\":%.9g}",
      event.scaleCalibrated ? "true" : "false",
      static_cast<long>(event.scaleOffsetCounts),
      static_cast<double>(event.scaleFactorCountsPerGram));
  if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(command.json)) {
    FS_LOGE(services::LogComponent::App,
            "Scale configuration serialization failed");
    return false;
  }
  command.jsonLength = static_cast<std::uint16_t>(length);
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=save_scale_config");
    return false;
  }
  return true;
}

void sendOverlay(rtos::RtosContext& ctx, rtos::UiCommandType type,
                 rtos::UiOverlayKind kind, std::uint32_t requestId,
                 const char* title, const char* text,
                 std::int32_t value = 0) {
  rtos::UiCommand command{};
  command.type = type;
  command.overlayKind = kind;
  command.requestId = requestId;
  command.value = value;
  std::snprintf(command.title, sizeof(command.title), "%s", title);
  std::snprintf(command.text, sizeof(command.text), "%s", text);
  if (sendUiCommand(ctx, command, "AppTask: overlay queue overflow")) {
    pendingOverlay = kind;
  }
}

constexpr const char* kSpoolmanRequiredTitle = "Spoolman nicht verbunden";
constexpr const char* kSpoolmanRequiredMessage =
    "Spoolman ist nicht verbunden.\n"
    "Diese Funktion ben\xC3\xB6tigt eine aktive Spoolman-Verbindung.";

bool spoolmanReady(const rtos::RtosContext& ctx) {
  (void)ctx;
  return models::spoolmanOperationsAvailable(currentSpoolmanAppState);
}

void publishSpoolmanAppState(rtos::RtosContext& ctx,
                             std::uint32_t requestId,
                             const char* version = nullptr) {
  const EventBits_t bits = xEventGroupGetBits(ctx.systemEventGroup);
  currentSpoolmanAppState = models::spoolmanAppState(
      (bits & rtos::EVENT_SPOOLMAN_READY) != 0,
      (bits & rtos::EVENT_SPOOLMAN_TAG_FIELD_READY) != 0);
  if (version != nullptr && version[0] != '\0') {
    std::snprintf(currentSpoolmanServerVersion,
                  sizeof(currentSpoolmanServerVersion), "%s", version);
  }
  if (currentSpoolmanAppState ==
      models::SpoolmanAppState::SpoolmanUnavailable) {
    std::snprintf(currentSpoolmanServerVersion,
                  sizeof(currentSpoolmanServerVersion), "-");
  }

  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::UpdateSpoolmanState;
  command.requestId = requestId;
  command.spoolmanAppState = currentSpoolmanAppState;
  std::snprintf(command.title, sizeof(command.title), "%s",
                currentSpoolmanServerVersion);
  switch (currentSpoolmanAppState) {
    case models::SpoolmanAppState::SpoolmanReady:
      std::snprintf(command.text, sizeof(command.text),
                    "Spoolman: online | NFC-Feld: bereit");
      break;
    case models::SpoolmanAppState::TagFieldUnavailable:
      std::snprintf(command.text, sizeof(command.text),
                    "Spoolman: online | NFC-Feld: nicht verf\xC3\xBCgbar");
      break;
    case models::SpoolmanAppState::SpoolmanUnavailable:
      std::snprintf(command.text, sizeof(command.text), "Spoolman: offline");
      break;
  }
  sendUiCommand(ctx, command, "AppTask: Spoolman AppState queue overflow");
}

bool requireSpoolman(rtos::RtosContext& ctx, std::uint32_t requestId,
                     const char* operation) {
  if (spoolmanReady(ctx)) return true;
  FS_LOGW(services::LogComponent::App,
          "Online operation blocked operation=%s reason=spoolman_offline",
          operation != nullptr ? operation : "unknown");
  sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
              rtos::UiOverlayKind::Error, requestId,
              kSpoolmanRequiredTitle, kSpoolmanRequiredMessage);
  return false;
}

void reportAssignmentWriteFailure(rtos::RtosContext& ctx,
                                  std::uint32_t requestId,
                                  const char* diagnostic,
                                  const char* userMessage = nullptr);

bool sendTagAssignmentCommand(rtos::RtosContext& ctx,
                              rtos::SpoolmanCommandType type,
                              rtos::SpoolId spoolId = 0) {
  rtos::SpoolmanCommand command{};
  command.type = type;
  command.requestId = pendingTagAssignment.requestId;
  command.spoolId = spoolId;
  command.tagIdentity = pendingTagAssignment.identity;
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

bool sendTagRemovalCommand(rtos::RtosContext& ctx,
                           rtos::SpoolmanCommandType type,
                           rtos::SpoolId spoolId = 0) {
  rtos::SpoolmanCommand command{};
  command.type = type;
  command.requestId = pendingTagRemoval.requestId;
  command.spoolId = spoolId;
  command.tagIdentity = pendingTagRemoval.identity;
  return xQueueSend(ctx.spoolmanCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

void finishServerOnlyRemoval(rtos::RtosContext& ctx) {
  const auto requestId = pendingTagRemoval.requestId;
  pendingTagRemoval = {};
  pendingTagOperation = PendingTagOperation::None;
  resolvedTagIdentity = {};
  resolvedTagSpoolId = 0;
  rtos::UiCommand hide{};
  hide.type = rtos::UiCommandType::HideProgress;
  sendUiCommand(ctx, hide, "AppTask: removal progress close overflow");
  rtos::UiCommand result{};
  result.type = rtos::UiCommandType::ShowScreen;
  result.screenId = rtos::UiScreenId::TagResult;
  result.requestId = requestId;
  std::snprintf(result.text, sizeof(result.text),
                "Tag-Zuordnung erfolgreich entfernt.\nOriginaler Taginhalt blieb unver\xC3\xA4ndert.");
  currentScreen = result.screenId;
  sendUiCommand(ctx, result, "AppTask: server-only removal result overflow");
}

void continueRemovalAfterSpoolmanUpdate(rtos::RtosContext& ctx) {
  resolvedTagIdentity = {};
  resolvedTagSpoolId = 0;
  if (!pendingTagRemoval.clearPayload) {
    finishServerOnlyRemoval(ctx);
    return;
  }
  if (!tagPresent || !removalTagMatches(currentTag) ||
      !currentTag.capabilities.canClearFilamentStationPayload) {
    const auto requestId = pendingTagRemoval.requestId;
    pendingTagRemoval = {};
    pendingTagOperation = PendingTagOperation::None;
    sendOverlay(
        ctx, rtos::UiCommandType::ShowDialog, rtos::UiOverlayKind::Error,
        requestId, "Zuordnung teilweise entfernt",
        "Die Spoolman-Zuordnung wurde entfernt.\nDie FilamentStation-Daten konnten nicht vom Tag entfernt werden.");
    return;
  }
  rtos::NfcCommand command{};
  command.type = rtos::NfcCommandType::EraseTag;
  command.requestId = pendingTagRemoval.requestId;
  if (xQueueSend(ctx.nfcCommandQueue, &command, pdMS_TO_TICKS(50)) != pdPASS) {
    const auto requestId = pendingTagRemoval.requestId;
    pendingTagRemoval = {};
    sendOverlay(
        ctx, rtos::UiCommandType::ShowDialog, rtos::UiOverlayKind::Error,
        requestId, "Zuordnung teilweise entfernt",
        "Die Spoolman-Zuordnung wurde entfernt.\nDer Auftrag zum Entfernen der FilamentStation-Daten konnte nicht gestartet werden.");
    return;
  }
  pendingTagOperation = PendingTagOperation::Erase;
  pendingTagRemoval.stage = TagRemovalStage::ClearingPayload;
  sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
              rtos::UiOverlayKind::TagWrite,
              pendingTagRemoval.requestId,
              "Tag-Zuordnung wird entfernt",
              "Spoolman-Zuordnung entfernt. FilamentStation-Daten werden vom Tag entfernt und verifiziert.");
}

void finishMappingOnlyAssignment(rtos::RtosContext& ctx, const char* text) {
  const auto requestId = pendingTagAssignment.requestId;
  const auto spoolId = pendingTagAssignment.spoolId;
  lastUsedTagSpoolId = spoolId;
  pendingTagAssignment = {};
  pendingTagOperation = PendingTagOperation::None;
  rtos::UiCommand hide{};
  hide.type = rtos::UiCommandType::HideProgress;
  sendUiCommand(ctx, hide, "AppTask: assignment progress close overflow");
  rtos::UiCommand result{};
  result.type = rtos::UiCommandType::ShowScreen;
  result.screenId = rtos::UiScreenId::TagResult;
  result.requestId = requestId;
  result.spoolId = spoolId;
  std::snprintf(result.text, sizeof(result.text), "%s", text);
  currentScreen = result.screenId;
  sendUiCommand(ctx, result, "AppTask: assignment result overflow");
}

void continueAssignmentAfterSpoolmanUpdate(rtos::RtosContext& ctx) {
  resolvedTagIdentity = pendingTagAssignment.identity;
  resolvedTagSpoolId = pendingTagAssignment.spoolId;
  if (!tagPresent || !assignmentTagMatches(currentTag)) {
    pendingTagAssignment.tagRemoved = true;
    reportAssignmentWriteFailure(
        ctx, pendingTagAssignment.requestId,
        "AssignTag Spoolman mapping stored but tag unavailable",
        "Tag wurde in Spoolman zugeordnet.\nDer Tag wurde entfernt oder ausgetauscht; die Tagdaten wurden nicht aktualisiert.");
    return;
  }
  if (!pendingTagAssignment.writePayload) {
    finishMappingOnlyAssignment(
        ctx, "Tag erfolgreich zugeordnet.\nOriginalinhalt wurde nicht ver\xC3\xA4ndert.");
    return;
  }
  if (currentTag.format == models::TagFormat::FilamentStation &&
      currentTag.definition.hasSpoolId &&
      currentTag.definition.spoolId == pendingTagAssignment.spoolId) {
    finishMappingOnlyAssignment(
        ctx, "Tag erfolgreich zugeordnet.\nDie Zuordnung war bereits auf dem Tag gespeichert.");
    return;
  }
  if (!currentTag.capabilities.canWriteFilamentStationPayload) {
    reportAssignmentWriteFailure(
        ctx, pendingTagAssignment.requestId,
        "AssignTag write capability changed after Spoolman update");
    return;
  }
  rtos::NfcCommand command{};
  command.type = rtos::NfcCommandType::WriteSpoolTag;
  command.requestId = pendingTagAssignment.requestId;
  command.spoolId = pendingTagAssignment.spoolId;
  if (xQueueSend(ctx.nfcCommandQueue, &command, pdMS_TO_TICKS(50)) != pdPASS) {
    reportAssignmentWriteFailure(ctx, pendingTagAssignment.requestId,
                                 "AssignTag NFC command queue full");
    return;
  }
  pendingTagSpoolId = pendingTagAssignment.spoolId;
  pendingTagOperation = PendingTagOperation::Write;
  pendingTagAssignment.stage = TagAssignmentStage::WritingPayload;
  sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
              rtos::UiOverlayKind::TagWrite,
              pendingTagAssignment.requestId, "Tag wird zugeordnet",
              "Spoolman-Zuordnung gespeichert. Der Tag wird aktualisiert und verifiziert.");
}

void reportAssignmentWriteFailure(rtos::RtosContext& ctx,
                                  std::uint32_t requestId,
                                  const char* diagnostic,
                                  const char* userMessage) {
  const rtos::SpoolId assignedSpoolId = pendingTagAssignment.spoolId;
  const std::uint32_t resultRequestId =
      requestId != 0 ? requestId : pendingTagAssignment.requestId;
  pendingTagOperation = PendingTagOperation::None;
  pendingTagAssignment = {};
  if (assignedSpoolId != 0) lastUsedTagSpoolId = assignedSpoolId;
  const char* detail = diagnostic != nullptr ? diagnostic : "unknown failure";
  constexpr char kLegacyPrefix[] = "AppTask: ";
  if (std::strncmp(detail, kLegacyPrefix, sizeof(kLegacyPrefix) - 1U) == 0)
    detail += sizeof(kLegacyPrefix) - 1U;
  FS_LOGE(services::LogComponent::App, "%s", detail);

  rtos::UiCommand hide{};
  hide.type = rtos::UiCommandType::HideProgress;
  sendUiCommand(ctx, hide,
                "AppTask: assignment write failure progress close overflow");
  sendOverlay(
      ctx, rtos::UiCommandType::ShowDialog, rtos::UiOverlayKind::Error,
      resultRequestId, "Tag teilweise zugeordnet",
      userMessage != nullptr
          ? userMessage
          : "Tag wurde zugeordnet.\nDie Zuordnung konnte jedoch nicht auf dem Tag gespeichert werden.\nEin erneuter Versuch ist m\xC3\xB6glich.");
}

// Re-sends the BootProgress overlay with whichever of bootSdStatus/
// bootNfcStatus/bootScaleStatus have actually been filled in so far. A
// no-op once Home has already been shown (startupNavigationSent) --
// updating the overlay after HideProgress fired would never be seen.
void refreshBootProgress(rtos::RtosContext& ctx, std::uint32_t requestId) {
  if (startupNavigationSent) return;
  char text[192];
  std::size_t used = static_cast<std::size_t>(
      std::snprintf(text, sizeof(text), "Display bereit."));
  const std::array<const char*, 3> lines{
      {bootSdStatus, bootNfcStatus, bootScaleStatus}};
  for (const char* line : lines) {
    if (line[0] == '\0' || used >= sizeof(text)) continue;
    const int written =
        std::snprintf(text + used, sizeof(text) - used, "\n%s", line);
    if (written > 0) used += static_cast<std::size_t>(written);
  }
  sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
              rtos::UiOverlayKind::BootProgress, requestId,
              "FilamentStation startet", text);
}

void showHomeWhenStartupReady(rtos::RtosContext& ctx) {
  if (startupNavigationSent || !uiStartupReady || !storageStartupReady) return;
  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::HideProgress;
  sendUiCommand(ctx, command, "AppTask: boot overlay queue overflow");
  command.type = rtos::UiCommandType::ShowScreen;
  command.screenId = rtos::UiScreenId::Home;
  if (sendUiCommand(ctx, command, "AppTask: startup navigation queue overflow")) {
    startupNavigationSent = true;
    currentScreen = rtos::UiScreenId::Home;
    previousScreen = rtos::UiScreenId::Home;
  }
}

const char* taskDiagnosticStateName(eTaskState state) {
  switch (state) {
    case eRunning: return "running";
    case eReady: return "ready";
    case eBlocked: return "blocked";
    case eSuspended: return "suspended";
    case eDeleted: return "deleted";
    default: return "invalid";
  }
}

struct DiagnosticsSummary {
  std::uint32_t worstStackFreeBytes = UINT32_MAX;
  char worstStackTaskName[16]{};
  UBaseType_t worstQueueWaiting = 0;
  UBaseType_t worstQueueCapacity = 0;
  char worstQueueName[16]{};
  EventBits_t eventBits = 0;
};

// Task-Diagnose (Phase 10.1): das EEZ-Layout hat nur ein einzelnes,
// 464x40px kleines Label fuer diese Daten -- zu wenig fuer eine
// detaillierte Aufschluesselung aller 9 Tasks/9 Queues. Der vollstaendige
// Bericht (jede Task/Queue einzeln, alle Event-Bits) geht daher als
// strukturierte Logzeilen an FS_LOGI (per Seriell/Logdatei einsehbar,
// gleiches Muster wie die erweiterte Bambu-Kommunikationsprotokollierung);
// die Rueckgabe liefert nur die fuer die Bildschirmanzeige relevante
// Zusammenfassung (jeweils der knappste Task/die vollste Queue).
DiagnosticsSummary logTaskDiagnostics(rtos::RtosContext& ctx) {
  // static: AppTask processes exactly one UI action at a time (single
  // FreeRTOS consumer loop, never re-entrant), so these locals do not need
  // per-call stack storage -- kept off AppTask's stack for the same reason
  // ScaleTask/NfcTask's AppEvent locals were made static earlier this
  // project (repeated stack-overflow crashes from deeply nested,
  // moderately large call-local data).
  static DiagnosticsSummary summary{};
  summary = {};

  struct TaskEntry {
    const char* name;
    TaskHandle_t handle;
    std::uint32_t configuredStackBytes;
  };
  static const std::array<TaskEntry, 9> tasks{{
      {"LoggingTask", ctx.loggingTask, config::kLoggingTask.stackSize},
      {"UiTask", ctx.uiTask, config::kUiTask.stackSize},
      {"AppTask", ctx.appTask, config::kAppTask.stackSize},
      {"ScaleTask", ctx.scaleTask, config::kScaleTask.stackSize},
      {"NfcTask", ctx.nfcTask, config::kNfcTask.stackSize},
      {"StorageTask", ctx.storageTask, config::kStorageTask.stackSize},
      {"NetworkTask", ctx.networkTask, config::kNetworkTask.stackSize},
      {"SpoolmanTask", ctx.spoolmanTask, config::kSpoolmanTask.stackSize},
      {"BambuTask", ctx.bambuTask, config::kBambuTask.stackSize},
  }};
  for (const auto& task : tasks) {
    if (task.handle == nullptr) continue;
    // uxTaskGetStackHighWaterMark() liefert auf dem ESP32-Xtensa-Port Bytes,
    // da StackType_t dort uint8_t ist (anders als auf vielen 32-Bit-Ports,
    // wo es Worte sind) -- keine Umrechnung noetig.
    const std::uint32_t freeBytes =
        static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(task.handle));
    const eTaskState state = eTaskGetState(task.handle);
    FS_LOGI(services::LogComponent::Rtos,
            "Task diagnostics name=%s state=%s stack_free_bytes=%lu "
            "stack_configured_bytes=%lu",
            task.name, taskDiagnosticStateName(state),
            static_cast<unsigned long>(freeBytes),
            static_cast<unsigned long>(task.configuredStackBytes));
    if (freeBytes < summary.worstStackFreeBytes) {
      summary.worstStackFreeBytes = freeBytes;
      std::snprintf(summary.worstStackTaskName,
                    sizeof(summary.worstStackTaskName), "%s", task.name);
    }
  }

  struct QueueEntry {
    const char* name;
    QueueHandle_t handle;
    UBaseType_t configuredLength;
  };
  static const std::array<QueueEntry, 9> queues{{
      {"AppEvent", ctx.appEventQueue, config::kAppEventQueueLength},
      {"UiCommand", ctx.uiCommandQueue, config::kUiCommandQueueLength},
      {"Scale", ctx.scaleCommandQueue, config::kServiceCommandQueueLength},
      {"Nfc", ctx.nfcCommandQueue, config::kServiceCommandQueueLength},
      {"Storage", ctx.storageCommandQueue, config::kStorageCommandQueueLength},
      {"Network", ctx.networkCommandQueue, config::kServiceCommandQueueLength},
      {"Spoolman", ctx.spoolmanCommandQueue, config::kServiceCommandQueueLength},
      {"Bambu", ctx.bambuCommandQueue, config::kServiceCommandQueueLength},
      {"Log", ctx.logQueue, config::kLogQueueLength},
  }};
  for (const auto& queue : queues) {
    if (queue.handle == nullptr) continue;
    const UBaseType_t waiting = uxQueueMessagesWaiting(queue.handle);
    FS_LOGI(services::LogComponent::Rtos,
            "Queue diagnostics name=%s waiting=%u capacity=%u", queue.name,
            static_cast<unsigned>(waiting),
            static_cast<unsigned>(queue.configuredLength));
    if (waiting > summary.worstQueueWaiting) {
      summary.worstQueueWaiting = waiting;
      summary.worstQueueCapacity = queue.configuredLength;
      std::snprintf(summary.worstQueueName, sizeof(summary.worstQueueName),
                    "%s", queue.name);
    }
  }

  summary.eventBits = xEventGroupGetBits(ctx.systemEventGroup);
  FS_LOGI(services::LogComponent::Rtos,
          "Event bit diagnostics bits=0x%03lX ui_ready=%d sd_ready=%d "
          "scale_ready=%d nfc_ready=%d wifi_connected=%d spoolman_ready=%d "
          "bambu_ready=%d fatal_error=%d spoolman_tag_field_ready=%d",
          static_cast<unsigned long>(summary.eventBits),
          (summary.eventBits & rtos::EVENT_UI_READY) != 0,
          (summary.eventBits & rtos::EVENT_SD_READY) != 0,
          (summary.eventBits & rtos::EVENT_SCALE_READY) != 0,
          (summary.eventBits & rtos::EVENT_NFC_READY) != 0,
          (summary.eventBits & rtos::EVENT_WIFI_CONNECTED) != 0,
          (summary.eventBits & rtos::EVENT_SPOOLMAN_READY) != 0,
          (summary.eventBits & rtos::EVENT_BAMBU_READY) != 0,
          (summary.eventBits & rtos::EVENT_FATAL_ERROR) != 0,
          (summary.eventBits & rtos::EVENT_SPOOLMAN_TAG_FIELD_READY) != 0);

  FS_LOGI(services::LogComponent::Rtos,
          "Memory diagnostics heap_free=%lu heap_min_free=%lu "
          "psram_free=%lu psram_min_free=%lu",
          static_cast<unsigned long>(ESP.getFreeHeap()),
          static_cast<unsigned long>(ESP.getMinFreeHeap()),
          static_cast<unsigned long>(ESP.getFreePsram()),
          static_cast<unsigned long>(ESP.getMinFreePsram()));

  return summary;
}

void handleUiAction(rtos::RtosContext& ctx, const rtos::UiAction& action) {
  rtos::UiCommand command{};
  command.requestId = action.requestId;
  command.printerId = action.printerId;
  command.spoolId = action.spoolId;
  command.amsId = action.amsId;
  command.trayId = action.trayId;
  command.value = action.value;

  if (rtos::requiresOnlineSpoolman(action.type) &&
      !requireSpoolman(ctx, action.requestId, "ui_action"))
    return;

  switch (action.type) {
    case rtos::UiActionType::AssignTag: {
      if (!tagPresent || !currentTag.capabilities.canAssociateByUid ||
          currentTag.identity.source == models::TagIdentitySource::Unknown ||
          currentTag.identity.value[0] == '\0' ||
          currentTag.uidLength == 0 ||
          currentTag.uidLength > pendingTagAssignment.uid.size()) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Zuordnung nicht m\xC3\xB6glich",
                    "Es ist kein zuordenbarer NFC-Tag vorhanden.");
        return;
      }
      const EventBits_t spoolmanBits = xEventGroupGetBits(ctx.systemEventGroup);
      if (!services::tagOperationsAvailable(
              (spoolmanBits & rtos::EVENT_SPOOLMAN_READY) != 0,
              (spoolmanBits & rtos::EVENT_SPOOLMAN_TAG_FIELD_READY) != 0)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Zuordnung nicht m\xC3\xB6glich",
                    "Spoolman oder das Textfeld extra.tag ist nicht bereit.");
        return;
      }
      if (pendingTagAssignment.stage != TagAssignmentStage::None) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Zuordnung l\xC3\xA4uft bereits",
                    "Bitte den laufenden NFC-Vorgang abschlie\xC3\x9F" "en.");
        return;
      }

      rtos::SpoolId spoolId = action.spoolId;
      if (action.value == 1) spoolId = lastUsedTagSpoolId;
      if (spoolId == 0 && currentTag.definition.hasSpoolId)
        spoolId = currentTag.definition.spoolId;

      pendingTagAssignment = {};
      pendingTagAssignment.stage = TagAssignmentStage::SelectingSpool;
      pendingTagAssignment.requestId = action.requestId;
      pendingTagAssignment.spoolId = spoolId;
      pendingTagAssignment.identity = currentTag.identity;
      pendingTagAssignment.uidLength = currentTag.uidLength;
      std::memcpy(pendingTagAssignment.uid.data(), currentTag.uid,
                  currentTag.uidLength);
      pendingTagAssignment.writePayload =
          nfc::assignmentEffect(currentTag) ==
          nfc::TagAssignmentEffect::MappingAndPayload;

      if (spoolId == 0) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::SpoolPicker, action.requestId,
                    "Spoolman-Spule ausw\xC3\xA4hlen", "");
        requestSpoolSearch(ctx, action.requestId);
        return;
      }

      rtos::UiAction selection = action;
      selection.type = rtos::UiActionType::SelectSpool;
      selection.spoolId = spoolId;
      handleUiAction(ctx, selection);
      return;
    }

    case rtos::UiActionType::RemoveTagAssignment: {
      if (!tagPresent || currentTag.uidLength == 0 ||
          currentTag.identity.source == models::TagIdentitySource::Unknown ||
          currentTag.identity.value[0] == '\0') {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Entfernen nicht m\xC3\xB6glich",
                    "Es ist kein zuordenbarer NFC-Tag vorhanden.");
        return;
      }
      if (pendingTagRemoval.stage != TagRemovalStage::None) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Entfernen l\xC3\xA4uft bereits",
                    "Bitte den laufenden NFC-Vorgang abschlie\xC3\x9F" "en.");
        return;
      }
      const EventBits_t spoolmanBits = xEventGroupGetBits(ctx.systemEventGroup);
      if (!services::tagOperationsAvailable(
              (spoolmanBits & rtos::EVENT_SPOOLMAN_READY) != 0,
              (spoolmanBits & rtos::EVENT_SPOOLMAN_TAG_FIELD_READY) != 0)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Entfernen nicht m\xC3\xB6glich",
                    "Spoolman oder das Textfeld extra.tag ist nicht bereit.");
        return;
      }

      pendingTagRemoval = {};
      pendingTagRemoval.stage = TagRemovalStage::LookingUp;
      pendingTagRemoval.requestId = action.requestId;
      pendingTagRemoval.identity = currentTag.identity;
      pendingTagRemoval.uidLength = currentTag.uidLength;
      std::memcpy(pendingTagRemoval.uid.data(), currentTag.uid,
                  currentTag.uidLength);
      pendingTagRemoval.clearPayload =
          nfc::removalEffect(currentTag) ==
          nfc::TagAssignmentEffect::MappingAndPayload;
      if (!sendTagRemovalCommand(ctx,
                                 rtos::SpoolmanCommandType::FindSpoolByTag)) {
        pendingTagRemoval = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Entfernen fehlgeschlagen",
                    "Die Anfrage konnte nicht an SpoolmanTask gesendet werden.");
        return;
      }
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                  "Tag-Zuordnung wird gepr\xC3\xBC" "ft",
                  "Die zugeordnete Spule wird in Spoolman gesucht.");
      return;
    }

    case rtos::UiActionType::Cancel:
      if ((wifiPortalRequested || wifiPortalActive) &&
          action.value == static_cast<std::int32_t>(
                              rtos::UiOverlayKind::ConnectionProgress)) {
        rtos::NetworkCommand networkCommand{};
        networkCommand.type = rtos::NetworkCommandType::StopPortal;
        networkCommand.requestId =
            wifiPortalRequestId != 0 ? wifiPortalRequestId : action.requestId;
        if (!sendNetworkCommand(ctx, networkCommand)) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Abbruch fehlgeschlagen",
                      "Der Abbruchauftrag konnte nicht an den NetworkTask gesendet werden.");
        }
        return;
      }
      if (pendingTagAssignment.stage ==
              TagAssignmentStage::SelectingSpool ||
          pendingTagAssignment.stage ==
              TagAssignmentStage::AwaitingReassignmentConfirmation)
        pendingTagAssignment = {};
      pendingServerReassignmentConfirmation = false;
      pendingUnlinkConfirmation = false;
      pendingClearStagingConfirmation = false;
      // Zustandsautomat (Phase 9.10): beide Picker-Flags haengen nur an
      // einem UI-seitigen Overlay ohne eigene Netzwerkanfrage in Flug --
      // ein Abbruch muss sie zuruecksetzen, sonst bleibt der jeweilige
      // Button danach dauerhaft wirkungslos (naechster Tastendruck haelt
      // den Picker faelschlich fuer bereits offen).
      pendingStagingSpoolSelection = false;
      if (pendingSlotAssignment.stage == SlotAssignmentStage::SelectingSpool)
        pendingSlotAssignment = {};
      if (pendingTagRemoval.stage == TagRemovalStage::AwaitingConfirmation ||
          pendingTagRemoval.stage == TagRemovalStage::LookingUp)
        pendingTagRemoval = {};
      if (currentScreen == rtos::UiScreenId::TagDefinitionImport ||
          currentScreen == rtos::UiScreenId::BambuSpoolType) {
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::Home;
        currentScreen = command.screenId;
        sendUiCommand(ctx, command, "AppTask: Bambu import cancel overflow");
        return;
      }
      if (currentScreen == rtos::UiScreenId::TagReview ||
          currentScreen == rtos::UiScreenId::TagWrite) {
        pendingTagOperation = PendingTagOperation::None;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId =
            previousScreen == rtos::UiScreenId::StagingActions ||
                    previousScreen == rtos::UiScreenId::TagLegacy
                ? previousScreen
                : rtos::UiScreenId::TagActionSelect;
        currentScreen = command.screenId;
        if (command.screenId == rtos::UiScreenId::StagingActions)
          applyTagUiState(command);
        sendUiCommand(ctx, command, "AppTask: NFC cancel navigation overflow");
        return;
      }
      command.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, command, "AppTask: hide overlay queue overflow");
      pendingOverlay = rtos::UiOverlayKind::None;
      quickWeight.pending = false;
      advancedWeight.pending = false;
      pendingTagOperation = PendingTagOperation::None;
      return;

    case rtos::UiActionType::Confirm: {
      if (pendingOverlay == rtos::UiOverlayKind::TagDefinitionImport) {
        command.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, command, "AppTask: Bambu import dialog close overflow");
        pendingOverlay = rtos::UiOverlayKind::None;
        rtos::UiAction assign = action;
        assign.type = rtos::UiActionType::AssignTag;
        assign.spoolId = 0;
        handleUiAction(ctx, assign);
        return;
      }
      if (currentScreen == rtos::UiScreenId::TagReview) {
        if (!requireSpoolman(ctx, action.requestId, "tag_assignment")) {
          pendingTagOperation = PendingTagOperation::None;
          return;
        }
        if (!tagPresent || pendingTagOperation == PendingTagOperation::None) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "NFC-Tag fehlt", "Der Tag wurde vor dem Start entfernt.");
          return;
        }
        rtos::NfcCommand nfcCommand{};
        nfcCommand.requestId = action.requestId;
        nfcCommand.spoolId = pendingTagSpoolId;
        nfcCommand.type = pendingTagOperation == PendingTagOperation::Write
                              ? rtos::NfcCommandType::WriteSpoolTag
                              : rtos::NfcCommandType::EraseTag;
        const bool mutationAllowed =
            nfcCommand.type == rtos::NfcCommandType::WriteSpoolTag
                ? nfc::mayWriteTag(currentTag)
                : nfc::mayEraseTag(currentTag);
        if (!mutationAllowed) {
          pendingTagOperation = PendingTagOperation::None;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Tag ist schreibgesch\xC3\xBCtzt",
                      "Originale Bambu-Tags werden niemals ver\xC3\xA4ndert.");
          return;
        }
        if (xQueueSend(ctx.nfcCommandQueue, &nfcCommand,
                       pdMS_TO_TICKS(50)) != pdPASS) {
          pendingTagOperation = PendingTagOperation::None;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "NFC-Auftrag fehlgeschlagen",
                      "Die NFC-Command-Queue ist voll.");
          return;
        }
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::TagWrite;
        currentScreen = command.screenId;
        sendUiCommand(ctx, command, "AppTask: NFC write screen queue overflow");
        return;
      }
      const auto confirmedOverlay = pendingOverlay;
      command.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, command, "AppTask: hide confirmed overlay queue overflow");
      pendingOverlay = rtos::UiOverlayKind::None;
      if (confirmedOverlay == rtos::UiOverlayKind::Confirmation &&
          pendingServerReassignmentConfirmation) {
        if (!requireSpoolman(ctx, action.requestId, "reassign_tag")) {
          pendingServerReassignmentConfirmation = false;
          pendingTagAssignment = {};
          return;
        }
        pendingServerReassignmentConfirmation = false;
        if (pendingTagAssignment.stage !=
            TagAssignmentStage::AwaitingReassignmentConfirmation ||
            pendingTagAssignment.previousSpoolId == 0) {
          pendingTagAssignment = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Zuordnung abgebrochen",
                      "Der Zuordnungsauftrag ist nicht mehr aktuell.");
          return;
        }
        pendingTagAssignment.stage = TagAssignmentStage::ClearingPrevious;
        if (!sendTagAssignmentCommand(ctx,
                                      rtos::SpoolmanCommandType::ClearSpoolTag,
                                      pendingTagAssignment.previousSpoolId)) {
          pendingTagAssignment = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Zuordnung fehlgeschlagen",
                      "Die bisherige Zuordnung konnte nicht zum L\xC3\xB6schen gesendet werden.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Zuordnung wird ersetzt",
                    "Die bisherige Spoolman-Zuordnung wird entfernt.");
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::Confirmation &&
          pendingUnlinkConfirmation) {
        if (!requireSpoolman(ctx, action.requestId,
                             "remove_tag_assignment")) {
          pendingUnlinkConfirmation = false;
          pendingTagRemoval = {};
          return;
        }
        pendingUnlinkConfirmation = false;
        if (pendingTagRemoval.stage !=
            TagRemovalStage::AwaitingConfirmation) {
          pendingTagRemoval = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Entfernen abgebrochen",
                      "Der Zuordnungsauftrag ist nicht mehr aktuell.");
          return;
        }
        if (!tagPresent || !removalTagMatches(currentTag)) {
          pendingTagRemoval = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Entfernen nicht m\xC3\xB6glich",
                      "Der Tag wurde entfernt oder ausgetauscht.");
          return;
        }
        pendingTagRemoval.stage = TagRemovalStage::ClearingServerAssignment;
        if (!sendTagRemovalCommand(ctx,
                                   rtos::SpoolmanCommandType::ClearSpoolTag,
                                   pendingTagRemoval.spoolId)) {
          pendingTagRemoval = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Entfernen fehlgeschlagen",
                      "Die Spoolman-Aktualisierung konnte nicht gestartet werden.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Tag-Zuordnung wird entfernt",
                    "Das Feld extra.tag der zugeordneten Spule wird geleert und verifiziert.");
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::Confirmation &&
          pendingClearStagingConfirmation) {
        pendingClearStagingConfirmation = false;
        stagingSpoolId = 0;
        command.type = rtos::UiCommandType::UpdateStaging;
        command.spoolId = 0;
        sendUiCommand(ctx, command, "AppTask: clear staging queue overflow");
        rtos::UiCommand toast{};
        toast.type = rtos::UiCommandType::ShowToast;
        toast.requestId = action.requestId;
        std::snprintf(toast.text, sizeof(toast.text), "Staging geleert");
        sendUiCommand(ctx, toast, "AppTask: clear staging toast overflow");
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::TagReview) {
        if (!requireSpoolman(ctx, action.requestId, "tag_assignment")) {
          pendingTagOperation = PendingTagOperation::None;
          return;
        }
        if (!tagPresent || pendingTagOperation == PendingTagOperation::None) {
          pendingTagOperation = PendingTagOperation::None;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "NFC-Tag fehlt", "Der Tag wurde vor dem Start entfernt.");
          return;
        }
        rtos::NfcCommand nfcCommand{};
        nfcCommand.requestId = action.requestId;
        nfcCommand.spoolId = pendingTagSpoolId;
        nfcCommand.type = pendingTagOperation == PendingTagOperation::Write
                              ? rtos::NfcCommandType::WriteSpoolTag
                              : rtos::NfcCommandType::EraseTag;
        const bool mutationAllowed =
            nfcCommand.type == rtos::NfcCommandType::WriteSpoolTag
                ? nfc::mayWriteTag(currentTag)
                : nfc::mayEraseTag(currentTag);
        if (!mutationAllowed) {
          pendingTagOperation = PendingTagOperation::None;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Tag ist schreibgesch\xC3\xBCtzt",
                      "Originale Bambu-Tags werden niemals ver\xC3\xA4ndert.");
          return;
        }
        if (xQueueSend(ctx.nfcCommandQueue, &nfcCommand,
                       pdMS_TO_TICKS(50)) != pdPASS) {
          pendingTagOperation = PendingTagOperation::None;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "NFC-Auftrag fehlgeschlagen",
                      "Die NFC-Command-Queue ist voll.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::TagWrite, action.requestId,
                    pendingTagOperation == PendingTagOperation::Write
                        ? "Tag wird zugeordnet"
                        : "Tag-Zuordnung wird entfernt",
                    "Tag am Leser belassen. Lesen, Schreiben und Verifikation laufen.");
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::QuickWeightConfirmation) {
        if (!requireSpoolman(ctx, action.requestId, "quick_weight_update")) {
          quickWeight.pending = false;
          return;
        }
        if (!quickWeight.pending) return;
        if (scaleError || !scaleStable) {
          quickWeight.pending = false;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Messung nicht best\xC3\xA4tigt",
                      "Der Messwert ist nicht mehr stabil. Bitte erneut wiegen.");
          return;
        }
        quickWeight.pending = false;
        weightUpdate = {};
        weightUpdate.active = true;
        weightUpdate.requestId = action.requestId;
        weightUpdate.update.spoolId = quickWeight.spoolId;
        weightUpdate.update.remainingWeightGrams =
            quickWeight.pendingRemainingWeightGrams;
        weightUpdate.update.emptySpoolWeightGrams =
            quickWeight.emptyWeightGrams;
        if (!sendWeightUpdate(ctx, action.requestId, weightUpdate.update)) {
          weightUpdate = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Gewicht nicht gespeichert",
                      "Die Spoolman-Anfrage konnte nicht gestartet werden. Bitte den Wiegevorgang erneut ausf\xC3\xBChren.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Gewicht speichern",
                    "Restgewicht wird an Spoolman gesendet und danach neu geladen.");
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::AdvancedWeightConfirmation) {
        if (!requireSpoolman(ctx, action.requestId,
                             "advanced_weight_update")) {
          advancedWeight.pending = false;
          return;
        }
        if (!advancedWeight.pending) return;
        advancedWeight.pending = false;
        weightUpdate = {};
        weightUpdate.active = true;
        weightUpdate.advanced = true;
        weightUpdate.requestId = action.requestId;
        weightUpdate.update.spoolId = advancedWeight.spoolId;
        weightUpdate.update.remainingWeightGrams =
            advancedWeight.remainingWeightGrams;
        weightUpdate.update.initialWeightGrams =
            advancedWeight.initialWeightGrams;
        weightUpdate.update.emptySpoolWeightGrams =
            advancedWeight.emptyWeightGrams;
        weightUpdate.update.updateInitialWeight = true;
        weightUpdate.update.updateEmptySpoolWeight = true;
        if (!sendWeightUpdate(ctx, action.requestId, weightUpdate.update)) {
          weightUpdate = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Gewicht nicht gespeichert",
                      "Die Spoolman-Anfrage konnte nicht gestartet werden. Bitte den Wiegevorgang erneut ausf\xC3\xBChren.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Gewicht speichern",
                    "Gewichte werden an Spoolman gesendet und danach neu geladen.");
        return;
      }
      const char* result = "Mock-Aktion best\xC3\xA4tigt; keine reale Funktion ausgef\xC3\xBChrt.";
      if (confirmedOverlay == rtos::UiOverlayKind::RestartConfirmation) {
        result = "Neustart best\xC3\xA4tigt; im Mock nicht ausgef\xC3\xBChrt.";
      } else if (confirmedOverlay == rtos::UiOverlayKind::WifiResetConfirmation) {
        rtos::NetworkCommand networkCommand{};
        networkCommand.type = rtos::NetworkCommandType::ClearCredentials;
        networkCommand.requestId = action.requestId;
        if (!sendNetworkCommand(ctx, networkCommand)) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "WLAN-Zugangsdaten",
                      "Der L\xC3\xB6schauftrag konnte nicht gesendet werden.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::ConnectionProgress,
                    action.requestId, "WLAN-Zugangsdaten",
                    "Gespeicherte Zugangsdaten werden gel\xC3\xB6scht.");
        return;
      }
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Success, action.requestId,
                  "Vorgang erfolgreich", result);
      return;
    }

    case rtos::UiActionType::SelectPrinter: {
      if (action.value == 1) {
        previousScreen = currentScreen;
        currentScreen = rtos::UiScreenId::PrinterSelect;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::PrinterSelect;
        sendUiCommand(ctx, command,
                      "AppTask: printer-select command queue overflow");
        return;
      }
      // printerId pruefen: the printer roster itself is not yet loaded into
      // AppTask (open gap, see printerCollection comment above), so this is
      // limited to rejecting the invalid-id sentinel.
      if (!models::isValidPrinterId(action.printerId)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Drucker w\xC3\xA4hlen", "Ung\xC3\xBCltige Drucker-ID.");
        return;
      }

      // wechseln + Zustand sichern: printerCollection keeps every printer's
      // last-known state keyed by id; switching only moves which entry is
      // marked active, it never clears or overwrites another printer's
      // data. Staging/Quick-/Advanced-Weight state below is likewise left
      // untouched on purpose (Staging erhalten).
      models::PrinterState* previousActive = models::findPrinter(
          printerCollection, printerCollection.activePrinterId);
      if (previousActive != nullptr) previousActive->isActive = false;
      models::PrinterState& target = printerEntry(action.printerId);
      target.isActive = true;
      printerCollection.activePrinterId = action.printerId;

      // Connect is idempotent (BambuTask refreshes instead of reconnecting
      // if already connected, see BambuTask::handleConnect), so this both
      // establishes the connection for a printer never connected this
      // session and refreshes an already-connected one -- covers switching
      // to a printer that was never auto-connected at boot.
      {
        const models::BambuPrinterConfig* storedConfig =
            models::findPrinterConfig(printerConfigs, action.printerId);
        rtos::BambuCommand connect{};
        connect.type = rtos::BambuCommandType::Connect;
        connect.requestId = action.requestId;
        connect.printerId = action.printerId;
        if (storedConfig != nullptr) connect.printerConfig = *storedConfig;
        sendBambuCommand(ctx, connect);
      }

      command.type = rtos::UiCommandType::UpdateHeader;
      if (!sendUiCommand(ctx, command,
                         "AppTask: header update command queue overflow")) {
        return;
      }
      rtos::UiCommand amsCommand = command;
      amsCommand.type = rtos::UiCommandType::UpdateAmsOverview;
      amsCommand.amsId = target.activeAmsId;
      sendUiCommand(ctx, amsCommand,
                    "AppTask: AMS overview command queue overflow");
      // Push whatever real AMS/tray data is already known immediately; the
      // RequestStatus above will refresh it further once the response
      // arrives.
      syncAmsToUi(ctx, action.printerId);

      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = previousScreen;
      currentScreen = previousScreen;
      sendUiCommand(ctx, command,
                    "AppTask: printer return command queue overflow");
      return;
    }

    case rtos::UiActionType::OpenSettings:
      if (currentScreen == rtos::UiScreenId::SettingsHome) {
        return;
      }
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::SettingsHome;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = rtos::UiScreenId::SettingsHome;
      sendUiCommand(ctx, command,
                    "AppTask: settings command queue overflow");
      return;

    case rtos::UiActionType::Back:
    case rtos::UiActionType::Close:
      command.type = rtos::UiCommandType::ShowScreen;
      if (currentScreen == rtos::UiScreenId::StagingActions) {
        // Leeres Staging: der Status-Screen wurde beim Reingehen
        // uebersprungen (siehe SelectStaging oben) -- "Zur\xC3\xBCck" geht
        // dann ebenso direkt zu Home statt zu StagingDetails.
        command.screenId = stagingSpoolId == 0 ? rtos::UiScreenId::Home
                                               : rtos::UiScreenId::StagingDetails;
        currentScreen = command.screenId;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::StagingDetails) {
        command.screenId = rtos::UiScreenId::Home;
        currentScreen = rtos::UiScreenId::Home;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::TrayActions) {
        command.screenId = rtos::UiScreenId::TrayDetails;
        currentScreen = rtos::UiScreenId::TrayDetails;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::TrayDetails) {
        command.screenId = rtos::UiScreenId::Home;
        currentScreen = rtos::UiScreenId::Home;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::TraySelect) {
        command.screenId = rtos::UiScreenId::StagingActions;
        currentScreen = rtos::UiScreenId::StagingActions;
        previousScreen = rtos::UiScreenId::StagingDetails;
      } else if (currentScreen == rtos::UiScreenId::SettingsSpoolman) {
        command.screenId = rtos::UiScreenId::SettingsHome;
        currentScreen = rtos::UiScreenId::SettingsHome;
      } else if (currentScreen == rtos::UiScreenId::SettingsPrinterEdit) {
        command.screenId = rtos::UiScreenId::SettingsPrinters;
        currentScreen = rtos::UiScreenId::SettingsPrinters;
      } else if (currentScreen == rtos::UiScreenId::SettingsPrinters) {
        command.screenId = printerSettingsReturnScreen;
        currentScreen = printerSettingsReturnScreen;
      } else if (currentScreen == rtos::UiScreenId::SettingsWifi ||
                 currentScreen == rtos::UiScreenId::SettingsScale ||
                 currentScreen == rtos::UiScreenId::SettingsDevice ||
                 currentScreen == rtos::UiScreenId::SettingsDiagnostics ||
                 currentScreen == rtos::UiScreenId::SettingsFirmware) {
        command.screenId = rtos::UiScreenId::SettingsHome;
        currentScreen = rtos::UiScreenId::SettingsHome;
      } else if (currentScreen == rtos::UiScreenId::TagReview ||
                 currentScreen == rtos::UiScreenId::TagWrite) {
        command.screenId =
            previousScreen == rtos::UiScreenId::StagingActions ||
                    previousScreen == rtos::UiScreenId::TagLegacy
                ? previousScreen
                : rtos::UiScreenId::TagActionSelect;
        currentScreen = command.screenId;
        pendingTagOperation = PendingTagOperation::None;
      } else if (currentScreen == rtos::UiScreenId::TagActionSelect ||
                 currentScreen == rtos::UiScreenId::TagResult) {
        command.screenId = rtos::UiScreenId::StagingActions;
        currentScreen = command.screenId;
      } else if (currentScreen == rtos::UiScreenId::TagUnknown) {
        command.screenId = rtos::UiScreenId::Home;
        currentScreen = command.screenId;
        previousScreen = command.screenId;
      } else {
        command.screenId = previousScreen;
        currentScreen = previousScreen;
      }
      if (command.screenId == rtos::UiScreenId::StagingActions)
        applyTagUiState(command);
      sendUiCommand(ctx, command, "AppTask: back command queue overflow");
      return;

    case rtos::UiActionType::OpenWifiSettings:
    case rtos::UiActionType::OpenScaleSettings:
    case rtos::UiActionType::OpenDeviceSettings:
    case rtos::UiActionType::OpenDiagnostics:
    case rtos::UiActionType::OpenFirmwareSettings: {
      switch (action.type) {
        case rtos::UiActionType::OpenWifiSettings:
          currentScreen = rtos::UiScreenId::SettingsWifi;
          break;
        case rtos::UiActionType::OpenScaleSettings:
          currentScreen = rtos::UiScreenId::SettingsScale;
          break;
        case rtos::UiActionType::OpenDeviceSettings:
          currentScreen = rtos::UiScreenId::SettingsDevice;
          break;
        case rtos::UiActionType::OpenDiagnostics:
          currentScreen = rtos::UiScreenId::SettingsDiagnostics;
          break;
        case rtos::UiActionType::OpenFirmwareSettings:
          currentScreen = rtos::UiScreenId::SettingsFirmware;
          break;
        default:
          break;
      }
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command,
                    "AppTask: settings navigation queue overflow");
      if (action.type == rtos::UiActionType::OpenWifiSettings) {
        rtos::NetworkCommand networkCommand{};
        networkCommand.type = rtos::NetworkCommandType::RequestStatus;
        networkCommand.requestId = 0;
        if (!sendNetworkCommand(ctx, networkCommand)) {
          FS_LOGW(services::LogComponent::App,
                  "Command enqueue failed queue=network op=request_status");
        }
      }
      return;
    }

    case rtos::UiActionType::RefreshDiagnostics: {
      const DiagnosticsSummary summary = logTaskDiagnostics(ctx);
      command.type = rtos::UiCommandType::ShowToast;
      command.value = 300 + static_cast<std::int32_t>(action.type);
      std::snprintf(
          command.text, sizeof(command.text),
          "Min. Stack: %s %lu B | Volltste Queue: %s %u/%u | Bits: 0x%03lX",
          summary.worstStackTaskName,
          static_cast<unsigned long>(summary.worstStackFreeBytes),
          summary.worstQueueName[0] != '\0' ? summary.worstQueueName : "-",
          static_cast<unsigned>(summary.worstQueueWaiting),
          static_cast<unsigned>(summary.worstQueueCapacity),
          static_cast<unsigned long>(summary.eventBits));
      sendUiCommand(ctx, command, "AppTask: diagnostics refresh queue overflow");
      return;
    }

    case rtos::UiActionType::StartWifiPortal:
    case rtos::UiActionType::ResetWifiCredentials:
    case rtos::UiActionType::PrepareRestart:
    case rtos::UiActionType::CheckFirmwareUpdate: {
      if (action.type == rtos::UiActionType::StartWifiPortal) {
        rtos::NetworkCommand networkCommand{};
        networkCommand.type = rtos::NetworkCommandType::StartPortal;
        networkCommand.requestId = action.requestId;
        if (!sendNetworkCommand(ctx, networkCommand)) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "WLAN-Portal nicht gestartet",
                      "Der Auftrag konnte nicht an den NetworkTask gesendet werden.");
          return;
        }
        wifiPortalRequested = true;
        wifiPortalRequestId = action.requestId;
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::ConnectionProgress,
                    action.requestId, "WLAN-Konfiguration",
                    "Das Konfigurationsportal wird gestartet.");
        return;
      }
      if (action.type == rtos::UiActionType::ResetWifiCredentials) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::WifiResetConfirmation,
                    action.requestId, "WLAN zur\xC3\xBC" "cksetzen?",
                    "Gespeicherte WLAN-Zugangsdaten wirklich entfernen?");
        return;
      }
      if (action.type == rtos::UiActionType::PrepareRestart) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::RestartConfirmation,
                    action.requestId, "Ger\xC3\xA4t neu starten?",
                    "Der Neustart wird erst nach Best\xC3\xA4tigung ausgel\xC3\xB6st (Mock)." );
        return;
      }
      command.type = rtos::UiCommandType::ShowToast;
      command.value = 300 + static_cast<std::int32_t>(action.type);
      const char* text = "Mock-Aktion vorgemerkt";
      if (action.type == rtos::UiActionType::StartWifiPortal) text = "WLAN-Konfiguration vorgemerkt";
      else if (action.type == rtos::UiActionType::ResetWifiCredentials) text = "WLAN-Zugangsdaten nicht zur\xC3\xBC" "ckgesetzt (Mock)";
      else if (action.type == rtos::UiActionType::PrepareRestart) text = "Neustart nicht ausgeführt (Mock)";
      else if (action.type == rtos::UiActionType::CheckFirmwareUpdate) text = "Update-Prüfung nicht ausgeführt (Mock)";
      std::snprintf(command.text, sizeof(command.text), "%s", text);
      sendUiCommand(ctx, command, "AppTask: settings mock action queue overflow");
      return;
    }

    case rtos::UiActionType::TareScale:
    case rtos::UiActionType::StartScaleCalibration:
    case rtos::UiActionType::ResetScaleCalibration: {
      rtos::ScaleCommand scaleCommand{};
      scaleCommand.requestId = action.requestId;
      if (action.type == rtos::UiActionType::TareScale) {
        scaleCommand.type = rtos::ScaleCommandType::Tare;
      } else if (action.type == rtos::UiActionType::StartScaleCalibration) {
        scaleCommand.type = rtos::ScaleCommandType::StartCalibration;
        scaleCommand.referenceWeightGrams = static_cast<float>(action.value);
      } else {
        scaleCommand.type = rtos::ScaleCommandType::ResetCalibration;
      }
      sendScaleCommand(ctx, scaleCommand);
      return;
    }

    case rtos::UiActionType::OpenPrinterSettings:
      printerSettingsReturnScreen = currentScreen;
      currentScreen = rtos::UiScreenId::SettingsPrinters;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: printer settings queue overflow");
      return;

    case rtos::UiActionType::AddPrinter: {
      const rtos::PrinterId newId = allocatePrinterId();
      if (!models::isValidPrinterId(newId)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Drucker hinzuf\xC3\xBCgen",
                    "Maximale Anzahl Drucker erreicht.");
        return;
      }
      loadPrinterDraft(newId);
      currentScreen = rtos::UiScreenId::SettingsPrinterEdit;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      command.printerId = printerDraft.id;
      sendUiCommand(ctx, command, "AppTask: printer editor queue overflow");
      // Must follow ShowScreen: the editor resets its own placeholder draft
      // when the screen opens, so the real values are pushed right after to
      // overwrite it (see sendPrinterDraftToUi).
      sendPrinterDraftToUi(ctx);
      return;
    }
    case rtos::UiActionType::EditPrinter:
      loadPrinterDraft(action.printerId);
      currentScreen = rtos::UiScreenId::SettingsPrinterEdit;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      command.printerId = printerDraft.id;
      sendUiCommand(ctx, command, "AppTask: printer editor queue overflow");
      sendPrinterDraftToUi(ctx);
      return;

    case rtos::UiActionType::SetActivePrinter: {
      models::BambuPrinterConfig* target =
          models::findPrinterConfig(printerConfigs, action.printerId);
      if (target == nullptr || !target->enabled) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Drucker aktivieren",
                    "Drucker ist unbekannt oder deaktiviert.");
        return;
      }
      setSelectedPrinterConfig(action.printerId);
      // Bridges into the Phase 8.4 runtime "currently focused" printer --
      // same concept, triggered from Settings instead of the Home printer
      // bar.
      models::PrinterState* previousActive = models::findPrinter(
          printerCollection, printerCollection.activePrinterId);
      if (previousActive != nullptr) previousActive->isActive = false;
      models::PrinterState& runtimeEntry = printerEntry(action.printerId);
      runtimeEntry.isActive = true;
      printerCollection.activePrinterId = action.printerId;

      char error[64]{};
      if (!persistPrinterConfigs(ctx, action.requestId, false, error,
                                 sizeof(error))) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Speichern fehlgeschlagen", error);
        return;
      }
      // Replaces the old opcode-based UpdatePrinterList mutation with an
      // absolute sync from printerConfigs (the real source of truth), and
      // explicitly refreshes the header the old opcode used to trigger as a
      // side effect.
      syncAllPrinterEntriesToUi(ctx);
      rtos::UiCommand header{};
      header.type = rtos::UiCommandType::UpdateHeader;
      header.printerId = action.printerId;
      sendUiCommand(ctx, header, "AppTask: active printer header overflow");
      return;
    }
    case rtos::UiActionType::TogglePrinterEnabled: {
      models::BambuPrinterConfig* target =
          models::findPrinterConfig(printerConfigs, action.printerId);
      if (target != nullptr) {
        target->enabled = !target->enabled;
        char error[64]{};
        if (!persistPrinterConfigs(ctx, action.requestId, false, error,
                                   sizeof(error))) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Speichern fehlgeschlagen", error);
        }
      }
      syncPrinterEntryToUi(ctx, action.printerId);
      return;
    }
    case rtos::UiActionType::SetDefaultPrinter: {
      if (models::findPrinterConfig(printerConfigs, action.printerId) !=
          nullptr) {
        setDefaultPrinterConfig(action.printerId);
        char error[64]{};
        if (!persistPrinterConfigs(ctx, action.requestId, false, error,
                                   sizeof(error))) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Speichern fehlgeschlagen", error);
        }
      }
      syncAllPrinterEntriesToUi(ctx);
      return;
    }
    case rtos::UiActionType::SelectManagedPrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 0;
      sendUiCommand(ctx, command, "AppTask: printer selection queue overflow");
      return;
    case rtos::UiActionType::DeletePrinter: {
      const bool removed = removePrinterConfig(action.printerId);
      rtos::BambuCommand reset{};
      reset.type = rtos::BambuCommandType::Reset;
      reset.requestId = action.requestId;
      reset.printerId = action.printerId;
      sendBambuCommand(ctx, reset);
      if (printerCollection.activePrinterId == action.printerId) {
        models::PrinterState* activeEntry = models::findPrinter(
            printerCollection, printerCollection.activePrinterId);
        if (activeEntry != nullptr) activeEntry->isActive = false;
        printerCollection.activePrinterId = models::kInvalidPrinterId;
      }
      if (removed) {
        char error[64]{};
        if (!persistPrinterConfigs(ctx, action.requestId, false, error,
                                   sizeof(error))) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "L\xC3\xB6schen fehlgeschlagen", error);
        }
      }
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 4;
      sendUiCommand(ctx, command, "AppTask: delete printer queue overflow");
      // Default may have been reassigned to a remaining printer.
      syncAllPrinterEntriesToUi(ctx);
      currentScreen = rtos::UiScreenId::SettingsPrinters;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: printer return queue overflow");
      return;
    }
    case rtos::UiActionType::EditPrinterField: {
      char* destination = printerField(action.value);
      const std::size_t capacity = printerFieldCapacity(action.value);
      if (destination == nullptr || capacity == 0) return;
      std::snprintf(destination, capacity, "%s", action.text);
      command.type = rtos::UiCommandType::UpdateSettings;
      command.value = action.value;
      command.value = 20 + action.value;
      std::snprintf(command.text, sizeof(command.text), "%s", destination);
      sendUiCommand(ctx, command, "AppTask: printer field queue overflow");
      return;
    }
    case rtos::UiActionType::TestPrinterConnection:
    case rtos::UiActionType::SavePrinterSettings: {
      const char* error = validatePrinterDraft();
      if (error != nullptr) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Eingabefehler", error);
        return;
      }
      models::BambuPrinterConfig draftConfig{};
      printerConfigFromDraft(draftConfig);

      if (action.type == rtos::UiActionType::TestPrinterConnection) {
        if (pendingPrinterTestRequestId != 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Bambu-Verbindung",
                      "Es l\xC3\xA4uft bereits ein Verbindungstest.");
          return;
        }
        rtos::BambuCommand test{};
        test.type = rtos::BambuCommandType::TestConnection;
        test.requestId = action.requestId;
        test.printerId = draftConfig.printerId;
        test.printerConfig = draftConfig;
        if (!sendBambuCommand(ctx, test)) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Bambu-Verbindung",
                      "Der Auftrag konnte nicht an BambuTask gesendet werden.");
          return;
        }
        pendingPrinterTestRequestId = action.requestId;
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::BambuConnection,
                    action.requestId, "Bambu-Verbindung",
                    "Verbindung zum gew\xC3\xA4hlten Drucker wird gepr\xC3\xBC" "ft.");
        return;
      }

      // SavePrinterSettings: enabled/default/selected are owned by the
      // dedicated list actions (TogglePrinterEnabled/SetDefaultPrinter/
      // SetActivePrinter), not by this form, so they are preserved across
      // the overwrite below.
      models::BambuPrinterConfig* target = upsertPrinterConfig(draftConfig.printerId);
      if (target == nullptr) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Speichern fehlgeschlagen",
                    "Maximale Anzahl Drucker erreicht.");
        return;
      }
      const bool wasEnabled = target->enabled;
      const bool wasDefault = target->isDefault;
      const bool wasSelected = target->isSelected;
      *target = draftConfig;
      target->enabled = wasEnabled;
      target->isDefault = wasDefault;
      target->isSelected = wasSelected;

      char saveError[64]{};
      if (!persistPrinterConfigs(ctx, action.requestId, true, saveError,
                                 sizeof(saveError))) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Speichern fehlgeschlagen", saveError);
        return;
      }
      pendingBambuSaveNotifyPrinterId = draftConfig.printerId;
      currentScreen = rtos::UiScreenId::SettingsPrinters;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: printer return queue overflow");
      return;
    }

    case rtos::UiActionType::OpenSpoolmanSettings:
      currentScreen = rtos::UiScreenId::SettingsSpoolman;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command,
                    "AppTask: Spoolman settings queue overflow");
      return;

    case rtos::UiActionType::EditSpoolmanSetting: {
      char* destination = spoolmanField(action.value);
      const std::size_t capacity = spoolmanFieldCapacity(action.value);
      if (destination == nullptr || capacity == 0) {
        return;
      }
      std::snprintf(destination, capacity, "%s", action.text);
      command.type = rtos::UiCommandType::UpdateSettings;
      std::snprintf(command.text, sizeof(command.text), "%s", destination);
      sendUiCommand(ctx, command,
                    "AppTask: Spoolman field update queue overflow");
      return;
    }

    case rtos::UiActionType::TestSpoolmanConnection:
    case rtos::UiActionType::SaveSpoolmanSettings: {
      const char* error = validateSpoolmanDraft();
      if (error != nullptr) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Eingabefehler", error);
      } else {
        models::SpoolmanSettings settings{};
        if (!spoolmanSettingsFromDraft(settings)) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Eingabefehler", "Die Server-URL konnte nicht normalisiert werden.");
          return;
        }
        if (action.type == rtos::UiActionType::TestSpoolmanConnection) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest,
                    action.requestId, "Spoolman-Anfrage",
                    "Serverstatus und Version werden abgefragt.");
          rtos::SpoolmanCommand spoolman{};
          spoolman.type = rtos::SpoolmanCommandType::HealthCheck;
          spoolman.requestId = action.requestId;
          spoolman.settings = settings;
          if (xQueueSend(ctx.spoolmanCommandQueue, &spoolman,
                         pdMS_TO_TICKS(1000)) != pdPASS) {
            sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                        rtos::UiOverlayKind::Error, action.requestId,
                        "Spoolman", "Die Anfrage konnte nicht gestartet werden.");
          }
        } else {
          JsonDocument document;
          document["schemaVersion"] = 1;
          document["updatedAt"] = "1970-01-01T00:00:00Z";
          document["documentType"] = "spoolman";
          document["enabled"] = settings.enabled;
          document["name"] = settings.name;
          document["serverUrl"] = settings.serverUrl;
          document["timeoutMs"] = settings.timeoutMs;
          rtos::StorageCommand storage{};
          storage.type = rtos::StorageCommandType::SaveJson;
          storage.requestId = action.requestId;
          storage.documentType = rtos::StorageDocumentType::Spoolman;
          std::snprintf(storage.path, sizeof(storage.path), "/config/spoolman.json");
          const std::size_t length = serializeJson(document, storage.json,
                                                   sizeof(storage.json));
          if (length == 0 || length >= sizeof(storage.json)) {
            sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                        rtos::UiOverlayKind::Error, action.requestId,
                        "Speichern fehlgeschlagen", "Konfiguration ist zu gro\xC3\x9F.");
            return;
          }
          storage.jsonLength = static_cast<std::uint16_t>(length);
          pendingSpoolmanSaveRequestId = action.requestId;
          if (xQueueSend(ctx.storageCommandQueue, &storage,
                         pdMS_TO_TICKS(1000)) != pdPASS) {
            pendingSpoolmanSaveRequestId = 0;
            sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                        rtos::UiOverlayKind::Error, action.requestId,
                        "Speichern fehlgeschlagen", "StorageTask ist nicht erreichbar.");
          }
        }
      }
      return;
    }

    case rtos::UiActionType::SelectAms:
      command.type = rtos::UiCommandType::UpdateAmsOverview;
      sendUiCommand(ctx, command,
                    "AppTask: AMS overview command queue overflow");
      return;

    case rtos::UiActionType::SelectTray:
      if (action.value == 0 || action.value == 1) {
        previousScreen = action.value == 1 ? rtos::UiScreenId::TrayDetails
                                            : currentScreen;
        currentScreen = action.value == 1 ? rtos::UiScreenId::TrayActions
                                           : rtos::UiScreenId::TrayDetails;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = currentScreen;
      } else {
        command.type = rtos::UiCommandType::UpdateTrayDetails;
      }
      sendUiCommand(ctx, command, "AppTask: tray command queue overflow");
      return;

    case rtos::UiActionType::ConfigureSlot: {
      // "Slot konfigurieren" auf StagingActions navigiert nur zur
      // Slot-Auswahl; der eigentliche Commit passiert erst beim Antippen
      // eines Slots dort (ConfigureSlotFromStaging, siehe unten).
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::TraySelect;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: tray-select queue overflow");
      return;
    }

    case rtos::UiActionType::ReapplySlot:
    // "Erneut anwenden" sendet dieselbe Zuordnung wie ConfigureSlotFromStaging,
    // nur ausgehend von der bereits im Slot bekannten Spule
    // (trayActionClicked fuellt action.spoolId mit selectedTraySpoolId statt
    // stagingState.spoolId) -- daher derselbe Commit-Pfad ohne Duplizierung.
    case rtos::UiActionType::ConfigureSlotFromStaging: {
      // Drucker/AMS/Slot/Spoolman-Spule pruefen, bevor irgendetwas gesendet
      // wird.
      if (!models::isValidPrinterId(action.printerId)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren", "Kein Drucker ausgew\xC3\xA4hlt.");
        return;
      }
      if (action.amsId == 0 ||
          action.amsId > models::kMaximumAmsPerPrinter ||
          action.trayId >= models::kSlotsPerAms) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren", "Ung\xC3\xBCltiger AMS-Slot.");
        return;
      }
      if (action.spoolId == 0) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren", "Keine Spule ausgew\xC3\xA4hlt.");
        return;
      }
      if (pendingSlotAssignment.stage != SlotAssignmentStage::None) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren",
                    "Es l\xC3\xA4uft bereits eine Slot-Zuordnung.");
        return;
      }
      // Spoolman-Spule/Daten: AppTask behaelt aufgeloeste Spulendaten nach
      // dem Staging nicht, daher hier erneut anfragen, um material/color
      // verbindlich zu bekommen.
      if (!requestStagingSpool(ctx, action.requestId, action.spoolId)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren",
                    "Spulendaten konnten nicht angefragt werden.");
        return;
      }
      pendingSlotAssignment.stage = SlotAssignmentStage::LoadingSpool;
      pendingSlotAssignment.requestId = action.requestId;
      pendingSlotAssignment.printerId = action.printerId;
      pendingSlotAssignment.amsId = action.amsId;
      pendingSlotAssignment.trayId = action.trayId;
      pendingSlotAssignment.spoolId = action.spoolId;
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                  "Slot konfigurieren", "Spulendaten werden geladen.");
      return;
    }

    case rtos::UiActionType::ResetSlot:
    case rtos::UiActionType::UntagSlot: {
      // Reset: physischen Slot am Drucker leeren (leeres trayType/Farbe,
      // spoolId 0). Untag: nur die lokale Spoolman-Zuordnung entfernen, der
      // physische Slot-Inhalt (trayType/Farbe) bleibt wie zuletzt vom
      // Drucker berichtet unveraendert -- beides teilt sich denselben
      // AssignTray-Commit-Pfad wie ConfigureSlotFromStaging (siehe dortige
      // Erfolgs-/Fehlerbehandlung), nur ohne Spoolman-Spule zu laden.
      if (!models::isValidPrinterId(action.printerId)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren", "Kein Drucker ausgew\xC3\xA4hlt.");
        return;
      }
      if (action.amsId == 0 ||
          action.amsId > models::kMaximumAmsPerPrinter ||
          action.trayId >= models::kSlotsPerAms) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren", "Ung\xC3\xBCltiger AMS-Slot.");
        return;
      }
      if (pendingSlotAssignment.stage != SlotAssignmentStage::None) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren",
                    "Es l\xC3\xA4uft bereits eine Slot-Zuordnung.");
        return;
      }
      // action.amsId ist die UI-seitige 1-basierte AMS-Nummer ("AMS 1"..
      // "AMS 4"); das Bambu-Protokoll zaehlt AMS-Einheiten dagegen 0-basiert
      // (siehe docs/bambu-protocol.md und PrinterState::amsUnits) -- ohne
      // diese Umrechnung zielte AssignTray auf eine am Drucker nicht
      // existierende AMS-Einheit und wurde stillschweigend ignoriert.
      const std::uint8_t amsIndex =
          static_cast<std::uint8_t>(action.amsId - 1U);
      rtos::BambuCommand clearTray{};
      clearTray.type = rtos::BambuCommandType::AssignTray;
      clearTray.requestId = action.requestId;
      clearTray.printerId = action.printerId;
      clearTray.amsId = amsIndex;
      clearTray.trayId = action.trayId;
      clearTray.spoolId = 0;
      if (action.type == rtos::UiActionType::UntagSlot) {
        const models::PrinterSlotStateData* slot = models::findSlot(
            printerEntry(action.printerId), amsIndex, action.trayId);
        if (slot != nullptr) {
          std::snprintf(clearTray.trayType, sizeof(clearTray.trayType), "%s",
                        slot->material);
          std::snprintf(clearTray.trayColorHex,
                        sizeof(clearTray.trayColorHex), "%s", slot->colorHex);
        }
      }
      // ResetSlot laesst trayType/trayColorHex bewusst leer (Slot ohne
      // Filament); UntagSlot sendet oben die unveraendert uebernommenen
      // aktuellen Werte, damit der physische Slot-Inhalt erhalten bleibt.
      if (!sendBambuCommand(ctx, clearTray)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot konfigurieren",
                    "Der Auftrag konnte nicht an den Drucker gesendet werden.");
        return;
      }
      pendingSlotAssignment = {};
      pendingSlotAssignment.stage = SlotAssignmentStage::WritingSlot;
      pendingSlotAssignment.requestId = action.requestId;
      pendingSlotAssignment.printerId = action.printerId;
      pendingSlotAssignment.amsId = action.amsId;
      pendingSlotAssignment.trayId = action.trayId;
      pendingSlotAssignment.spoolId = 0;
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::BambuConnection, action.requestId,
                  action.type == rtos::UiActionType::ResetSlot
                      ? "Slot zur\xC3\xBC" "cksetzen"
                      : "Zuordnung entfernen",
                  "Wird an den Drucker \xC3\xBC" "bertragen.");
      return;
    }

    case rtos::UiActionType::RefreshSlot: {
      if (!models::isValidPrinterId(action.printerId)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot aktualisieren", "Kein Drucker ausgew\xC3\xA4hlt.");
        return;
      }
      rtos::BambuCommand statusRequest{};
      statusRequest.type = rtos::BambuCommandType::RequestStatus;
      statusRequest.requestId = action.requestId;
      statusRequest.printerId = action.printerId;
      if (!sendBambuCommand(ctx, statusRequest)) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Slot aktualisieren",
                    "Die Statusanfrage konnte nicht gesendet werden.");
        return;
      }
      command.type = rtos::UiCommandType::ShowToast;
      std::snprintf(command.text, sizeof(command.text),
                    "Status wird aktualisiert");
      sendUiCommand(ctx, command, "AppTask: refresh slot toast overflow");
      return;
    }

    case rtos::UiActionType::SelectStaging:
      if (action.value == 1) {
        // Explizite Weiterleitung (z. B. von StagingDetails) direkt zu
        // StagingActions.
        previousScreen = rtos::UiScreenId::StagingDetails;
        currentScreen = rtos::UiScreenId::StagingActions;
      } else if (stagingSpoolId == 0) {
        // Leeres Staging: der Status-Screen (StagingDetails) haette nichts
        // anzuzeigen -- direkt zu StagingActions springen. "Zur\xC3\xBCck"
        // geht dann zu Home statt zum uebersprungenen Status-Screen (siehe
        // die passende Anpassung im Back-Handler unten).
        previousScreen = rtos::UiScreenId::Home;
        currentScreen = rtos::UiScreenId::StagingActions;
      } else {
        previousScreen = currentScreen;
        currentScreen = rtos::UiScreenId::StagingDetails;
      }
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      if (currentScreen == rtos::UiScreenId::StagingActions)
        applyTagUiState(command);
      sendUiCommand(ctx, command,
                    "AppTask: staging screen command queue overflow");
      return;

    case rtos::UiActionType::ImportTagDefinition: {
      if ((currentTag.format != models::TagFormat::BambuLab &&
           currentTag.format != models::TagFormat::OpenPrintTag &&
           currentTag.format != models::TagFormat::OpenTag3D &&
           currentTag.format != models::TagFormat::Legacy) ||
          !currentTag.knownFormat || !currentTag.payloadValid) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Import nicht m\xC3\xB6glich",
                    "Es liegt keine g\xC3\xBCltige offene Tagdefinition vor.");
        return;
      }
      rtos::SpoolmanCommand spoolman{};
      spoolman.type = rtos::SpoolmanCommandType::ImportTagDefinition;
      spoolman.requestId = action.requestId;
      spoolman.tagDefinition = currentTag.definition;
      if (xQueueSend(ctx.spoolmanCommandQueue, &spoolman,
                     pdMS_TO_TICKS(1000)) != pdPASS) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Import fehlgeschlagen",
                    "Der Auftrag konnte nicht an SpoolmanTask gesendet werden.");
        return;
      }
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                  "Spoolman-Import",
                  "Die Tagdefinition wird \xC3\xBC" "bertragen.");
      return;
    }

    case rtos::UiActionType::ClearStaging:
    case rtos::UiActionType::SearchSpool:
    case rtos::UiActionType::SelectSpool:
      // Configure Manually (Phase 9.9): "Manuell" auf TrayActions oeffnet
      // den Spoolman-Spulenpicker fuer genau diesen Slot; die Auswahl commit-
      // tet ueber denselben Pfad wie ConfigureSlotFromStaging.
      // pendingSlotAssignment merkt sich amsId/trayId/printerId waehrend der
      // Picker offen ist (der generische Picker traegt selbst keinen
      // Slot-Kontext).
      if (action.type == rtos::UiActionType::SelectSpool &&
          pendingSlotAssignment.stage == SlotAssignmentStage::SelectingSpool) {
        if (action.spoolId == 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Zuordnung fehlgeschlagen",
                      "Keine g\xC3\xBCltige Spule ausgew\xC3\xA4hlt.");
          pendingSlotAssignment = {};
          return;
        }
        command.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, command,
                      "AppTask: manual slot picker close overflow");
        rtos::UiAction commit = action;
        commit.type = rtos::UiActionType::ConfigureSlotFromStaging;
        commit.printerId = pendingSlotAssignment.printerId;
        commit.amsId = pendingSlotAssignment.amsId;
        commit.trayId = pendingSlotAssignment.trayId;
        commit.spoolId = action.spoolId;
        pendingSlotAssignment = {};
        handleUiAction(ctx, commit);
        return;
      }
      if (action.type == rtos::UiActionType::SelectSpool &&
          currentScreen == rtos::UiScreenId::TrayActions &&
          pendingSlotAssignment.stage == SlotAssignmentStage::None) {
        if (!models::isValidPrinterId(action.printerId)) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Slot konfigurieren", "Kein Drucker ausgew\xC3\xA4hlt.");
          return;
        }
        if (action.amsId == 0 ||
            action.amsId > models::kMaximumAmsPerPrinter ||
            action.trayId >= models::kSlotsPerAms) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Slot konfigurieren", "Ung\xC3\xBCltiger AMS-Slot.");
          return;
        }
        pendingSlotAssignment = {};
        pendingSlotAssignment.stage = SlotAssignmentStage::SelectingSpool;
        pendingSlotAssignment.printerId = action.printerId;
        pendingSlotAssignment.amsId = action.amsId;
        pendingSlotAssignment.trayId = action.trayId;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::SpoolPicker, action.requestId,
                    "Spoolman-Spule ausw\xC3\xA4hlen", "");
        if (!requestSpoolSearch(ctx, action.requestId)) {
          pendingSlotAssignment = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Spoolman-Suche",
                      "Die Spulenauswahl konnte nicht geladen werden.");
        }
        return;
      }
      if (action.type == rtos::UiActionType::SelectSpool &&
          currentScreen == rtos::UiScreenId::StagingActions) {
        if (!pendingStagingSpoolSelection) {
          pendingStagingSpoolSelection = true;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::SpoolPicker, action.requestId,
                      "Spoolman-Spule ausw\xC3\xA4hlen", "");
          if (!requestSpoolSearch(ctx, action.requestId)) {
            pendingStagingSpoolSelection = false;
            sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                        rtos::UiOverlayKind::Error, action.requestId,
                        "Spoolman-Suche",
                        "Die Spulenauswahl konnte nicht geladen werden.");
          }
          return;
        }
        if (action.spoolId == 0) return;
        pendingStagingSpoolSelection = false;
        pendingStagingSpoolRequestId = action.requestId;
        command.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, command, "AppTask: staging picker close overflow");
        if (!requestStagingSpool(ctx, action.requestId, action.spoolId)) {
          pendingStagingSpoolRequestId = 0;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Spule laden",
                      "Die ausgew\xC3\xA4hlte Spule konnte nicht angefordert werden.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Spule laden", "Spoolman-Daten werden geladen.");
        return;
      }
      if (action.type == rtos::UiActionType::SearchSpool &&
          action.value >= 10) {
        const std::int32_t rawFilter = action.value - 10;
        const auto filter = rawFilter == 1
                                ? rtos::SpoolmanSearchFilter::Material
                            : rawFilter == 2
                                ? rtos::SpoolmanSearchFilter::Vendor
                            : rawFilter == 3
                                ? rtos::SpoolmanSearchFilter::Id
                                : rtos::SpoolmanSearchFilter::FilamentName;
        if (!requestSpoolSearch(ctx, action.requestId, action.text, filter))
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Spoolman-Suche", "Die Suche konnte nicht gestartet werden.");
        return;
      }
      if (action.type == rtos::UiActionType::SearchSpool &&
          (currentScreen == rtos::UiScreenId::TagDefinitionImport ||
           currentScreen == rtos::UiScreenId::TagLegacy ||
           currentScreen == rtos::UiScreenId::TagUnknown)) {
        rtos::UiAction assign = action;
        assign.type = rtos::UiActionType::AssignTag;
        assign.spoolId = 0;
        handleUiAction(ctx, assign);
        return;
      }
      if (action.type == rtos::UiActionType::SelectSpool &&
          pendingTagAssignment.stage == TagAssignmentStage::SelectingSpool) {
        if (action.spoolId == 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Zuordnung fehlgeschlagen",
                      "Keine g\xC3\xBCltige Spule ausgew\xC3\xA4hlt.");
          return;
        }
        pendingTagAssignment.requestId = action.requestId;
        pendingTagAssignment.spoolId = action.spoolId;
        pendingTagAssignment.stage = TagAssignmentStage::LookingUp;
        command.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, command, "AppTask: assignment picker close overflow");
        if (!sendTagAssignmentCommand(ctx,
                                      rtos::SpoolmanCommandType::FindSpoolByTag)) {
          pendingTagAssignment = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Zuordnung fehlgeschlagen",
                      "Die Anfrage konnte nicht an SpoolmanTask gesendet werden.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Tag wird zugeordnet",
                    "Bestehende Spoolman-Zuordnung wird gepr\xC3\xBC" "ft.");
        return;
      }
      if (action.type == rtos::UiActionType::SearchSpool &&
          currentScreen == rtos::UiScreenId::TagActionSelect) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::SpoolPicker, action.requestId,
                    "Spoolman-Spule ausw\xC3\xA4hlen", "");
        requestSpoolSearch(ctx, action.requestId);
        return;
      }
      // A spool selected in the picker supplies the semantic assignment
      // workflow with its target Spoolman ID.
      if (action.type == rtos::UiActionType::SelectSpool &&
          currentScreen == rtos::UiScreenId::TagActionSelect) {
        pendingTagSpoolId = action.spoolId != 0 ? action.spoolId
                                                : lastUsedTagSpoolId;
        if (pendingTagSpoolId == 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Keine Spule ausgew\xC3\xA4hlt",
                      "Es ist noch keine zuletzt verwendete Spule vorhanden.");
          return;
        }
        pendingTagOperation = PendingTagOperation::Write;
        command.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, command, "AppTask: spool picker close queue overflow");
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::TagReview;
        std::snprintf(command.text, sizeof(command.text),
                      "Tag: nativer NTAG21x\nSpoolman-ID: %lu\nPayload: spoolman:%lu\nAktion: schreiben und verifizieren",
                      static_cast<unsigned long>(pendingTagSpoolId),
                      static_cast<unsigned long>(pendingTagSpoolId));
        previousScreen = currentScreen;
        currentScreen = command.screenId;
        sendUiCommand(ctx, command, "AppTask: NFC review screen queue overflow");
        return;
      }
      if (action.type == rtos::UiActionType::ClearStaging) {
        if (action.spoolId == 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Staging leeren", "Im Staging ist keine Spule ausgew\xC3\xA4hlt.");
          return;
        }
        pendingClearStagingConfirmation = true;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Confirmation, action.requestId,
                    "Staging leeren",
                    "Die gestagte Spule wird aus dem Staging entfernt.\nSpoolman und der NFC-Tag werden dabei nicht ver\xC3\xA4ndert.");
        return;
      }
      command.type = rtos::UiCommandType::ShowToast;
      std::snprintf(command.text, sizeof(command.text),
                    "Staging-Aktion vorgemerkt (Spule %lu)",
                    static_cast<unsigned long>(action.spoolId));
      sendUiCommand(ctx, command,
                    "AppTask: staging action queue overflow");
      return;

    case rtos::UiActionType::AdvancedWeight: {
      if (action.value == 0) {
        if (action.spoolId == 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Keine Spule", "Im Staging ist keine Spule ausgew\xC3\xA4hlt.");
          return;
        }
        if (scaleError || !scaleCalibrated || !scaleStable) {
          sendOverlay(ctx,
                      scaleError || !scaleCalibrated
                          ? rtos::UiCommandType::ShowDialog
                          : rtos::UiCommandType::ShowProgress,
                      scaleError || !scaleCalibrated
                          ? rtos::UiOverlayKind::Error
                          : rtos::UiOverlayKind::WeightStabilizing,
                      action.requestId, "Waage nicht bereit",
                      scaleError
                          ? "Der HX711 liefert keine Messwerte."
                          : (!scaleCalibrated
                                 ? "Die Waage ist nicht kalibriert."
                                 : "Der reale Messwert ist noch instabil."));
          return;
        }
        const bool retainCommitted =
            advancedWeight.committed && advancedWeight.spoolId == action.spoolId;
        float emptyWeight = 0.0F;
        float initialWeight = 0.0F;
        char spoolName[32]{};
        if (std::sscanf(action.text, "%31[^|]|%f|%f", spoolName,
                        &emptyWeight, &initialWeight) != 3) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Spulendaten fehlen",
                      "Leer- oder Ausgangsgewicht ist nicht verf\xC3\xBCgbar.");
          return;
        }
        advancedWeight.pending = false;
        advancedWeight.mode = 0;
        advancedWeight.spoolId = action.spoolId;
        advancedWeight.grossWeightGrams = scaleWeightGrams();
        if (!retainCommitted) {
          advancedWeight.emptyWeightGrams = emptyWeight;
          advancedWeight.initialWeightGrams = initialWeight;
        }
        std::snprintf(advancedWeight.spoolName,
                      sizeof(advancedWeight.spoolName), "%s", spoolName);
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::AdvancedWeightMode,
                    action.requestId, "Erweitertes Wiegen", "");
        return;
      }

      if (action.value < 1 || action.value > 4 || advancedWeight.spoolId == 0)
        return;
      advancedWeight.mode = action.value;
      if ((action.value == 3 || action.value == 4) && action.text[0] == '\0') {
        char currentValue[24];
        std::snprintf(currentValue, sizeof(currentValue), "%.1f",
                      static_cast<double>(action.value == 3
                                              ? advancedWeight.emptyWeightGrams
                                              : advancedWeight.initialWeightGrams));
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::AdvancedWeightInput,
                    action.requestId,
                    action.value == 3 ? "Neues Leergewicht in g"
                                      : "Ausgangsgewicht in g (0 = l\xC3\xB6schen)",
                    currentValue, action.value);
        return;
      }

      if (action.value == 3 || action.value == 4) {
        char* end = nullptr;
        const float entered = std::strtof(action.text, &end);
        const float minimum = action.value == 3 ? 1.0F : 0.0F;
        if (action.text[0] == '\0' || end == nullptr || *end != '\0' ||
            entered < minimum || entered > 100000.0F) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Ung\xC3\xBCltige Eingabe",
                      action.value == 3 ? "Leergewicht muss 1 bis 100000 g sein."
                                        : "Ausgangsgewicht muss 0 bis 100000 g sein.");
          return;
        }
        if (action.value == 3)
          advancedWeight.emptyWeightGrams = entered;
        else
          advancedWeight.initialWeightGrams = entered;
      } else if (action.value == 2) {
        advancedWeight.initialWeightGrams =
            advancedWeight.grossWeightGrams - advancedWeight.emptyWeightGrams;
        if (advancedWeight.initialWeightGrams < 0.0F)
          advancedWeight.initialWeightGrams = 0.0F;
      }
      advancedWeight.remainingWeightGrams =
          advancedWeight.grossWeightGrams - advancedWeight.emptyWeightGrams;
      if (advancedWeight.remainingWeightGrams < 0.0F)
        advancedWeight.remainingWeightGrams = 0.0F;
      advancedWeight.pending = true;
      char summary[128];
      std::snprintf(summary, sizeof(summary),
                    "#%lu %s\nBrutto: %.1f g\nLeergewicht: %.1f g\nAusgangsgewicht: %.1f g\nRestgewicht: %.1f g\nBest\xC3\xA4tigen?",
                    static_cast<unsigned long>(advancedWeight.spoolId),
                    advancedWeight.spoolName,
                    static_cast<double>(advancedWeight.grossWeightGrams),
                    static_cast<double>(advancedWeight.emptyWeightGrams),
                    static_cast<double>(advancedWeight.initialWeightGrams),
                    static_cast<double>(advancedWeight.remainingWeightGrams));
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::AdvancedWeightConfirmation,
                  action.requestId, "Erweitertes Wiegen - Zusammenfassung", summary);
      return;
    }

    case rtos::UiActionType::QuickWeight: {
      if (action.spoolId == 0) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Keine Spule", "Im Staging ist keine Spule ausgew\xC3\xA4hlt.");
        return;
      }
      if (scaleError || !scaleCalibrated) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Waage nicht bereit",
                    scaleError ? "Der HX711 liefert keine Messwerte."
                               : "Die Waage ist nicht kalibriert.");
        return;
      }
      if (!scaleStable) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::WeightStabilizing, action.requestId,
                    "Gewicht stabilisieren",
                    "Der reale Messwert ist noch instabil.");
        return;
      }
      quickWeight.pending = true;
      quickWeight.requestId = action.requestId;
      quickWeight.spoolId = action.spoolId;
      quickWeight.emptyWeightGrams =
          action.value > 0 ? static_cast<float>(action.value) : 0.0F;
      quickWeight.pendingGrossWeightGrams = scaleWeightGrams();
      quickWeight.pendingRemainingWeightGrams =
          quickWeight.pendingGrossWeightGrams - quickWeight.emptyWeightGrams;
      if (quickWeight.pendingRemainingWeightGrams < 0.0F)
        quickWeight.pendingRemainingWeightGrams = 0.0F;
      std::snprintf(quickWeight.spoolName, sizeof(quickWeight.spoolName), "%s",
                    action.text[0] == '\0' ? "Spule" : action.text);
      char summary[96];
      if (quickWeight.hasLastMeasurement &&
          quickWeight.lastMeasurementSpoolId == quickWeight.spoolId) {
        std::snprintf(summary, sizeof(summary),
                      "#%lu %s\nBrutto: %.1f g | Rest: %.1f g\nZuletzt: %.1f g\nBest\xC3\xA4tigen?",
                      static_cast<unsigned long>(quickWeight.spoolId),
                      quickWeight.spoolName,
                      static_cast<double>(quickWeight.pendingGrossWeightGrams),
                      static_cast<double>(quickWeight.pendingRemainingWeightGrams),
                      static_cast<double>(quickWeight.lastMeasurementGrams));
      } else {
        std::snprintf(summary, sizeof(summary),
                      "#%lu %s\nBrutto: %.1f g | Rest: %.1f g\nZuletzt: keine\nBest\xC3\xA4tigen?",
                      static_cast<unsigned long>(quickWeight.spoolId),
                      quickWeight.spoolName,
                      static_cast<double>(quickWeight.pendingGrossWeightGrams),
                      static_cast<double>(quickWeight.pendingRemainingWeightGrams));
      }
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::QuickWeightConfirmation,
                  action.requestId, "Schnellwiegen - stabil", summary);
      return;
    }

    default:
      FS_LOGW(services::LogComponent::App, "UI action unhandled action=%u",
              static_cast<unsigned>(action.type));
      return;
  }
}

}  // namespace

void appTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  // One receiver owns this buffer for the complete task lifetime. Keeping the
  // comparatively large value message in static storage avoids consuming the
  // task stack before event-specific handlers run.
  static rtos::AppEvent event{};
  UBaseType_t reportedMinimumStack = static_cast<UBaseType_t>(~0U);
  for (;;) {
    if (xQueueReceive(ctx.appEventQueue, &event, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    if (event.type == rtos::AppEventType::UiAction) {
      handleUiAction(ctx, event.uiAction);
    } else if (event.type == rtos::AppEventType::ScaleMeasurement) {
      scaleCounts = event.value;
      scaleError = false;
      sendScaleUiState(ctx, event.requestId);
    } else if (event.type == rtos::AppEventType::ScaleStable ||
               event.type == rtos::AppEventType::ScaleUnstable) {
      scaleCounts = event.value;
      scaleStable = event.type == rtos::AppEventType::ScaleStable;
      scaleError = false;
      sendScaleUiState(ctx, event.requestId);
    } else if (event.type == rtos::AppEventType::ScaleTared ||
               event.type == rtos::AppEventType::ScaleCalibrated ||
               event.type == rtos::AppEventType::ScaleCalibrationReset) {
      scaleOffsetCounts = event.scaleOffsetCounts;
      scaleFactorCountsPerGram = event.scaleFactorCountsPerGram;
      scaleCalibrated = event.scaleCalibrated;
      scaleStable = false;
      scaleError = false;
      persistScaleConfiguration(ctx, event);
      sendScaleUiState(ctx, event.requestId);
      rtos::UiCommand result{};
      result.type = rtos::UiCommandType::ShowToast;
      result.requestId = event.requestId;
      result.value = 300 + static_cast<std::int32_t>(
          event.type == rtos::AppEventType::ScaleTared
              ? rtos::UiActionType::TareScale
              : (event.type == rtos::AppEventType::ScaleCalibrated
                     ? rtos::UiActionType::StartScaleCalibration
                     : rtos::UiActionType::ResetScaleCalibration));
      std::snprintf(result.text, sizeof(result.text), "%s",
                    event.type == rtos::AppEventType::ScaleTared
                        ? "Waage tariert"
                        : (event.type == rtos::AppEventType::ScaleCalibrated
                               ? "Kalibrierung gespeichert"
                               : "Kalibrierung zur\xC3\xBC" "ckgesetzt"));
      sendUiCommand(ctx, result, "AppTask: scale result UI queue overflow");
    } else if (event.type == rtos::AppEventType::ScaleReady ||
               event.type == rtos::AppEventType::ScaleError) {
      if (event.type == rtos::AppEventType::ScaleReady) {
        scaleCounts = event.value;
        scaleError = false;
      } else {
        scaleError = true;
        scaleStable = false;
      }
      sendScaleUiState(ctx, event.requestId);
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "Scale");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status, "AppTask: scale status UI queue overflow");
      if (event.type == rtos::AppEventType::ScaleError &&
          event.requestId != 0) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Waagenaktion fehlgeschlagen", event.text);
      }
      // ScaleReady repeats on every measurement (not a one-shot init
      // signal like NfcInitialized), so only the first occurrence updates
      // the boot overlay -- otherwise every live weight reading would
      // needlessly re-send it.
      if (bootScaleStatus[0] == '\0') {
        std::snprintf(bootScaleStatus, sizeof(bootScaleStatus),
                      event.type == rtos::AppEventType::ScaleReady
                          ? "Waage: bereit"
                          : "Waage: Fehler");
        refreshBootProgress(ctx, event.requestId);
      }
    } else if (event.type == rtos::AppEventType::NfcInitialized ||
               event.type == rtos::AppEventType::NfcTagDetected ||
               event.type == rtos::AppEventType::NfcTagRemoved ||
               event.type == rtos::AppEventType::NfcTagRead ||
               event.type == rtos::AppEventType::NfcTagWritten ||
               event.type == rtos::AppEventType::NfcTagErased ||
               event.type == rtos::AppEventType::NfcError) {
      if (event.type == rtos::AppEventType::NfcTagRead) {
        currentTag = event.tagReadResult;
        tagPresent = true;
        const bool nativeTechnology =
            currentTag.technology == models::TagTechnology::Ntag213 ||
            currentTag.technology == models::TagTechnology::Ntag215 ||
            currentTag.technology == models::TagTechnology::Ntag216;
        const bool nativeFormat =
            currentTag.format == models::TagFormat::EmptyNdef ||
            currentTag.format == models::TagFormat::FilamentStation;
        const bool nativePayload =
            nativeTechnology &&
            currentTag.format == models::TagFormat::FilamentStation &&
            currentTag.definition.hasSpoolId;
        const bool resolutionKnown =
            currentTag.identity.source == resolvedTagIdentity.source &&
            currentTag.identity.source != models::TagIdentitySource::Unknown &&
            std::strcmp(currentTag.identity.value,
                        resolvedTagIdentity.value) == 0;
        if (!nativePayload && !resolutionKnown &&
            currentTag.identity.source != models::TagIdentitySource::Unknown &&
            currentTag.identity.value[0] != '\0') {
          const EventBits_t bits = xEventGroupGetBits(ctx.systemEventGroup);
          const EventBits_t required =
              rtos::EVENT_SPOOLMAN_READY |
              rtos::EVENT_SPOOLMAN_TAG_FIELD_READY;
          if ((bits & required) == required) {
            pendingTagResolution = {};
            pendingTagResolution.active = true;
            pendingTagResolution.requestId = event.requestId;
            pendingTagResolution.identity = currentTag.identity;
            pendingTagResolution.uidLength = currentTag.uidLength;
            std::memcpy(pendingTagResolution.uid.data(), currentTag.uid,
                        currentTag.uidLength);
            rtos::SpoolmanCommand lookup{};
            lookup.type = rtos::SpoolmanCommandType::FindSpoolByTag;
            lookup.requestId = event.requestId;
            lookup.tagIdentity = currentTag.identity;
            if (xQueueSend(ctx.spoolmanCommandQueue, &lookup,
                           pdMS_TO_TICKS(1000)) == pdPASS)
              continue;
            pendingTagResolution = {};
          }
        }
        if (nativeTechnology && nativeFormat) {
          if (currentTag.definition.hasSpoolId)
            pendingTagSpoolId = currentTag.definition.spoolId;
          if (currentTag.format == models::TagFormat::FilamentStation &&
              currentTag.definition.hasSpoolId &&
              currentTag.identity.source !=
                  models::TagIdentitySource::Unknown &&
              currentTag.identity.value[0] != '\0') {
            const EventBits_t bits = xEventGroupGetBits(ctx.systemEventGroup);
            if ((bits & (rtos::EVENT_SPOOLMAN_READY |
                         rtos::EVENT_SPOOLMAN_TAG_FIELD_READY)) ==
                (rtos::EVENT_SPOOLMAN_READY |
                 rtos::EVENT_SPOOLMAN_TAG_FIELD_READY)) {
              pendingNativeConsistency = {};
              pendingNativeConsistency.active = true;
              pendingNativeConsistency.requestId = event.requestId;
              pendingNativeConsistency.payloadSpoolId =
                  currentTag.definition.spoolId;
              pendingNativeConsistency.identity = currentTag.identity;
              pendingNativeConsistency.uidLength = currentTag.uidLength;
              std::memcpy(pendingNativeConsistency.uid.data(), currentTag.uid,
                          currentTag.uidLength);
              rtos::SpoolmanCommand lookup{};
              lookup.type = rtos::SpoolmanCommandType::FindSpoolByTag;
              lookup.requestId = event.requestId;
              lookup.tagIdentity = currentTag.identity;
              if (xQueueSend(ctx.spoolmanCommandQueue, &lookup,
                             pdMS_TO_TICKS(1000)) == pdPASS) {
                sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                            rtos::UiOverlayKind::SpoolmanRequest,
                            event.requestId, "Tag-Zuordnung wird gepr\xC3\xBC" "ft",
                            "NFC-Payload und Spoolman-Zuordnung werden verglichen.");
                continue;
              }
              pendingNativeConsistency = {};
            }
            resolvedTagIdentity = {};
            resolvedTagSpoolId = 0;
            showNativeTagAction(
                ctx, event.requestId, 0,
                "Spoolman-Zuordnung nicht pr\xC3\xBC" "fbar. NDEF wird nicht als Zuordnung verwendet.");
          } else {
            showNativeTagAction(ctx, event.requestId,
                                mappedNfcSpool(currentTag),
                                mappedNfcSpool(currentTag) == 0
                                    ? "Nicht zugeordnet"
                                    : "Zugeordnet");
          }
        } else if (currentTag.format == models::TagFormat::BambuLab) {
          const rtos::SpoolId mappedSpool = mappedNfcSpool(currentTag);
          if (mappedSpool != 0) {
            rtos::UiCommand result{};
            result.type = rtos::UiCommandType::ShowScreen;
            result.screenId = rtos::UiScreenId::TagResult;
            result.spoolId = mappedSpool;
            std::snprintf(result.text, sizeof(result.text),
                          "Bambu-Tag read-only: vorhandene Zuordnung zu Spule %lu verwendet.",
                          static_cast<unsigned long>(mappedSpool));
            currentScreen = result.screenId;
            sendUiCommand(ctx, result, "AppTask: Bambu mapped result overflow");

            // Staging (Phase 9.3): TagResult only carries spoolId/text for
            // display -- stagingActionClicked (Schnell/Erweitert wiegen on
            // this screen) reads stagingState.spoolId/stagingSpoolState, not
            // command.spoolId. Without loading the resolved spool into
            // Staging here, "wiegen" from a mapped Bambu tag would silently
            // act on whatever spool happened to already be staged. Same
            // pattern as showNativeTagAction's Phase 9.2 fix.
            if (pendingStagingSpoolRequestId == 0) {
              if (requestStagingSpool(ctx, event.requestId, mappedSpool))
                pendingStagingSpoolRequestId = event.requestId;
            }
          } else {
            char summary[128]{};
            std::snprintf(summary, sizeof(summary),
                          "Hersteller: %s\nFilament: %s\nMaterial: %s\nFarbe: %s\nNenngewicht: %.0f g\nTag bleibt read-only.",
                          currentTag.definition.vendor,
                          currentTag.definition.filamentName,
                          currentTag.definition.material,
                          currentTag.definition.colorCode,
                          static_cast<double>(currentTag.definition.nominalFilamentWeightG));
            rtos::UiCommand definition{};
            definition.type = rtos::UiCommandType::ShowScreen;
            definition.screenId = rtos::UiScreenId::TagDefinitionImport;
            definition.requestId = event.requestId;
            std::snprintf(definition.text, sizeof(definition.text), "%s",
                          summary);
            if (sendUiCommand(ctx, definition,
                              "AppTask: Bambu definition screen overflow")) {
              previousScreen = currentScreen;
              currentScreen = definition.screenId;
            }
          }
        } else if (currentTag.format == models::TagFormat::OpenPrintTag ||
                   currentTag.format == models::TagFormat::OpenTag3D) {
          const char* formatName =
              currentTag.format == models::TagFormat::OpenPrintTag
                  ? "OpenPrintTag"
                  : "OpenTag3D";
          const rtos::SpoolId mappedSpool = mappedNfcSpool(currentTag);
          if (mappedSpool != 0) {
            rtos::UiCommand result{};
            result.type = rtos::UiCommandType::ShowScreen;
            result.screenId = rtos::UiScreenId::TagResult;
            result.spoolId = mappedSpool;
            std::snprintf(result.text, sizeof(result.text),
                          "%s read-only: Zuordnung zu Spule %lu verwendet.",
                          formatName,
                          static_cast<unsigned long>(mappedSpool));
            currentScreen = result.screenId;
            sendUiCommand(ctx, result,
                          "AppTask: open tag mapped result overflow");

            // Staging (Phase 9.4): same gap/fix as the Phase 9.3 Bambu path
            // -- TagResult only carries spoolId/text for display,
            // stagingActionClicked (Schnell/Erweitert wiegen) reads
            // stagingState.spoolId/stagingSpoolState, not command.spoolId.
            if (pendingStagingSpoolRequestId == 0) {
              if (requestStagingSpool(ctx, event.requestId, mappedSpool))
                pendingStagingSpoolRequestId = event.requestId;
            }
          } else {
            char summary[128]{};
            std::snprintf(summary, sizeof(summary),
                          "%s\nHersteller: %s\nFilament: %s\nMaterial: %s\nFarbe: %s\nGewicht: %.0fg / Leer: %.0fg",
                          formatName,
                          currentTag.definition.vendor,
                          currentTag.definition.filamentName,
                          currentTag.definition.material,
                          currentTag.definition.colorCode,
                          static_cast<double>(currentTag.definition.nominalFilamentWeightG),
                          static_cast<double>(currentTag.definition.emptySpoolWeightG));
            rtos::UiCommand definition{};
            definition.type = rtos::UiCommandType::ShowScreen;
            definition.screenId = rtos::UiScreenId::TagDefinitionImport;
            definition.requestId = event.requestId;
            std::snprintf(definition.text, sizeof(definition.text), "%s",
                          summary);
            if (sendUiCommand(ctx, definition,
                              "AppTask: open tag definition overflow")) {
              previousScreen = currentScreen;
              currentScreen = definition.screenId;
            }
          }
        } else if (currentTag.format == models::TagFormat::Legacy) {
          char uid[32]{};
          formatTagUid(currentTag, uid, sizeof(uid));
          rtos::UiCommand legacy{};
          legacy.type = rtos::UiCommandType::ShowScreen;
          legacy.screenId = rtos::UiScreenId::TagLegacy;
          legacy.requestId = event.requestId;
          applyTagUiState(legacy);
          std::snprintf(
              legacy.text, sizeof(legacy.text),
              legacy.spoolId == 0
                  ? "Format: Legacy\nSpoolman-ID: %lu\nTechnologie: %s\nUID: %s\nNicht zugeordnet"
                  : "Format: Legacy\nSpoolman-ID: %lu\nTechnologie: %s\nUID: %s\nZugeordnet zu Spule #%lu",
              static_cast<unsigned long>(currentTag.definition.spoolId),
              tagTechnologyName(currentTag.technology), uid,
              static_cast<unsigned long>(legacy.spoolId));
          previousScreen = currentScreen;
          currentScreen = legacy.screenId;
          sendUiCommand(ctx, legacy, "AppTask: legacy screen overflow");
        } else if (currentTag.format == models::TagFormat::Unknown) {
          const rtos::SpoolId mappedSpool = mappedNfcSpool(currentTag);
          if (mappedSpool != 0) {
            rtos::UiCommand result{};
            result.type = rtos::UiCommandType::ShowScreen;
            result.screenId = rtos::UiScreenId::TagResult;
            result.spoolId = mappedSpool;
            std::snprintf(result.text, sizeof(result.text),
                          "Unbekannter Tag ist per UID mit Spule %lu verbunden. Der Tag bleibt unver\xC3\xA4ndert.",
                          static_cast<unsigned long>(mappedSpool));
            previousScreen = currentScreen;
            currentScreen = result.screenId;
            sendUiCommand(ctx, result, "AppTask: unknown mapped result overflow");

            // Staging (Phase 9.7): same gap/fix as the Phase 9.3/9.4 mapped-
            // spool TagResult paths -- stagingActionClicked (Schnell/
            // Erweitert wiegen) reads stagingState.spoolId/
            // stagingSpoolState, not command.spoolId.
            if (pendingStagingSpoolRequestId == 0) {
              if (requestStagingSpool(ctx, event.requestId, mappedSpool))
                pendingStagingSpoolRequestId = event.requestId;
            }
          } else {
            char uid[32]{};
            formatTagUid(currentTag, uid, sizeof(uid));
            const char* writable = currentTag.physicalWritableKnown
                                       ? currentTag.physicalWritable
                                             ? "physisch beschreibbar; Aktionen gesperrt"
                                             : "physisch gesperrt"
                                       : "nicht sicher bestimmbar";
            rtos::UiCommand unknown{};
            unknown.type = rtos::UiCommandType::ShowScreen;
            unknown.screenId = rtos::UiScreenId::TagUnknown;
            unknown.requestId = event.requestId;
            applyTagUiState(unknown);
            std::snprintf(
                unknown.text, sizeof(unknown.text),
                "Technologie: %s\nUID: %s\nNDEF: %s\nSchreibf\xC3\xA4higkeit: %s\nNicht zugeordnet",
                tagTechnologyName(currentTag.technology), uid,
                currentTag.ndefPresent
                    ? currentTag.ndefReadable ? "vorhanden, lesbar"
                                              : "vorhanden, nicht lesbar"
                    : "nicht vorhanden",
                writable);
            previousScreen = currentScreen;
            currentScreen = unknown.screenId;
            sendUiCommand(ctx, unknown, "AppTask: unknown screen overflow");
          }
        }
      } else if (event.type == rtos::AppEventType::NfcTagRemoved) {
        const bool removalMatchesCurrentTag =
            tagPresent && currentTag.uidLength == event.nfcUidLength &&
            std::memcmp(currentTag.uid, event.nfcUid,
                        event.nfcUidLength) == 0;
        if (!removalMatchesCurrentTag) {
          FS_LOGW(services::LogComponent::App,
                  "NFC removal ignored reason=stale_uid");
          continue;
        }
        const bool operationWasPending =
            pendingTagOperation != PendingTagOperation::None ||
            pendingTagAssignment.stage != TagAssignmentStage::None;
        const bool assignmentWriteWasPending =
            pendingTagAssignment.stage ==
            TagAssignmentStage::WritingPayload;
        const bool removalPayloadWasPending =
            pendingTagRemoval.stage == TagRemovalStage::ClearingPayload;
        const bool removalConfirmationWasPending =
            pendingTagRemoval.stage ==
            TagRemovalStage::AwaitingConfirmation;
        const bool removalServerWasPending =
            pendingTagRemoval.stage == TagRemovalStage::LookingUp ||
            pendingTagRemoval.stage ==
                TagRemovalStage::ClearingServerAssignment;
        const bool assignmentServerUpdateWasPending =
            pendingTagAssignment.stage == TagAssignmentStage::LookingUp ||
            pendingTagAssignment.stage == TagAssignmentStage::ClearingPrevious ||
            pendingTagAssignment.stage == TagAssignmentStage::SettingTarget ||
            pendingTagAssignment.stage == TagAssignmentStage::RollingBackPrevious;
        if (assignmentServerUpdateWasPending) {
          pendingTagAssignment.tagRemoved = true;
        } else if (assignmentWriteWasPending) {
          pendingTagAssignment.tagRemoved = true;
        } else {
          pendingTagAssignment = {};
        }
        pendingNativeConsistency = {};
        pendingTagResolution = {};
        resolvedTagIdentity = {};
        resolvedTagSpoolId = 0;
        tagPresent = false;
        currentTag = {};
        pendingTagOperation = PendingTagOperation::None;
        pendingUnlinkConfirmation = false;
        if (removalConfirmationWasPending) pendingTagRemoval = {};
        if (removalServerWasPending) pendingTagRemoval.tagRemoved = true;
        if (assignmentWriteWasPending) {
          reportAssignmentWriteFailure(
              ctx, event.requestId,
              "AppTask: AssignTag payload write failed because tag was removed; mapping retained",
              "Tag wurde zugeordnet.\nDer Tag wurde w\xC3\xA4hrend des Vorgangs entfernt. Die Tagdaten wurden nicht aktualisiert.");
        } else if (removalPayloadWasPending) {
          pendingTagRemoval = {};
          FS_LOGE(services::LogComponent::App,
                  "Tag assignment removal partial mapping_removed=true payload_cleared=false reason=tag_removed");
          rtos::UiCommand hide{};
          hide.type = rtos::UiCommandType::HideProgress;
          sendUiCommand(ctx, hide,
                        "AppTask: removal progress close overflow");
          sendOverlay(
              ctx, rtos::UiCommandType::ShowDialog,
              rtos::UiOverlayKind::Error, event.requestId,
              "Zuordnung teilweise entfernt",
              "Die Tag-Zuordnung wurde entfernt.\nDie FilamentStation-Daten konnten nicht vom Tag entfernt werden.");
        } else if (operationWasPending && !assignmentServerUpdateWasPending) {
          rtos::UiCommand hide{};
          hide.type = rtos::UiCommandType::HideProgress;
          sendUiCommand(ctx, hide, "AppTask: removed-tag progress close overflow");
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, event.requestId,
                      "NFC-Vorgang abgebrochen",
                      "Der Tag wurde w\xC3\xA4hrend des Vorgangs entfernt.");
        }
      } else if (event.type == rtos::AppEventType::NfcTagWritten) {
        const bool assignmentWrite =
            pendingTagAssignment.stage ==
            TagAssignmentStage::WritingPayload;
        if (assignmentWrite &&
            (event.nfcUidLength != pendingTagAssignment.uidLength ||
             std::memcmp(event.nfcUid, pendingTagAssignment.uid.data(),
                         pendingTagAssignment.uidLength) != 0)) {
          reportAssignmentWriteFailure(
              ctx, event.requestId,
              "AppTask: AssignTag UID verification failed; mapping retained",
              "Der urspr\xC3\xBCngliche Tag wurde zugeordnet.\nDie UID hat sich w\xC3\xA4hrend des Vorgangs ge\xC3\xA4ndert. Die Tagdaten wurden nicht aktualisiert.");
          continue;
        }
        lastUsedTagSpoolId = event.spoolId;
        const models::TagReadResult previousTag = currentTag;
        currentTag = event.tagReadResult;
        currentTag.technology = previousTag.technology;
        currentTag.ndefPresent = true;
        currentTag.ndefReadable = true;
        currentTag.physicalWritableKnown =
            previousTag.physicalWritableKnown;
        currentTag.physicalWritable = previousTag.physicalWritable;
        currentTag.uidLength = event.nfcUidLength;
        std::memcpy(currentTag.uid, event.nfcUid, event.nfcUidLength);
        nfc::updateTagCapabilities(currentTag);
        tagPresent = true;
        pendingTagOperation = PendingTagOperation::None;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide, "AppTask: NFC write progress close overflow");
        rtos::UiCommand result{};
        result.type = rtos::UiCommandType::ShowScreen;
        result.screenId = rtos::UiScreenId::TagResult;
        result.requestId = event.requestId;
        result.spoolId = event.spoolId;
        if (assignmentWrite) {
          std::snprintf(
              result.text, sizeof(result.text),
              "Tag erfolgreich Spule %lu zugeordnet und beschrieben.\nUID und Payload wurden verifiziert.",
              static_cast<unsigned long>(event.spoolId));
          pendingTagAssignment = {};
        } else {
          std::snprintf(result.text, sizeof(result.text),
                        "Tag erfolgreich mit Spule %lu verbunden. UID und Payload wurden verifiziert.",
                        static_cast<unsigned long>(event.spoolId));
        }
        currentScreen = result.screenId;
        sendUiCommand(ctx, result, "AppTask: NFC result screen queue overflow");
      } else if (event.type == rtos::AppEventType::NfcTagErased) {
        const bool assignmentRemoval =
            pendingTagRemoval.stage == TagRemovalStage::ClearingPayload;
        if (assignmentRemoval &&
            (event.nfcUidLength != pendingTagRemoval.uidLength ||
             std::memcmp(event.nfcUid, pendingTagRemoval.uid.data(),
                         pendingTagRemoval.uidLength) != 0)) {
          pendingTagOperation = PendingTagOperation::None;
          pendingTagRemoval = {};
          FS_LOGE(services::LogComponent::App,
                  "Tag assignment removal verification failed mapping_removed=true reason=uid_mismatch");
          sendOverlay(
              ctx, rtos::UiCommandType::ShowDialog,
              rtos::UiOverlayKind::Error, event.requestId,
              "Zuordnung teilweise entfernt",
              "Die Tag-Zuordnung wurde entfernt.\nDie UID bei der L\xC3\xB6schverifikation stimmt nicht \xC3\xBC" "berein.");
          continue;
        }
        currentTag.format = models::TagFormat::EmptyNdef;
        currentTag.knownFormat = true;
        currentTag.payloadValid = true;
        currentTag.writable = currentTag.physicalWritable;
        currentTag.erasable = currentTag.physicalWritable;
        nfc::updateTagCapabilities(currentTag);
        currentTag.definition = {};
        currentTag.definition.format = models::TagFormat::EmptyNdef;
        pendingTagOperation = PendingTagOperation::None;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide, "AppTask: NFC erase progress close overflow");
        rtos::UiCommand result{};
        result.type = rtos::UiCommandType::ShowScreen;
        result.screenId = rtos::UiScreenId::TagResult;
        result.requestId = event.requestId;
        std::snprintf(
            result.text, sizeof(result.text),
            assignmentRemoval
                ? "Tag-Zuordnung entfernt.\nFilamentStation-Daten wurden ebenfalls vom Tag entfernt und die L\xC3\xB6schung wurde verifiziert."
                : "NFC-Tag gel\xC3\xB6scht. Der leere NDEF-Zustand wurde verifiziert.");
        if (assignmentRemoval) pendingTagRemoval = {};
        if (assignmentRemoval)
          FS_LOGI(services::LogComponent::App,
                  "Tag assignment removed mapping_removed=true payload_cleared=true verified=true");
        currentScreen = result.screenId;
        sendUiCommand(ctx, result, "AppTask: NFC erase result queue overflow");
      } else if (event.type == rtos::AppEventType::NfcError &&
                 pendingTagOperation != PendingTagOperation::None) {
        if (pendingTagAssignment.stage ==
            TagAssignmentStage::WritingPayload) {
          reportAssignmentWriteFailure(
              ctx, event.requestId,
              "AppTask: AssignTag payload write or verification failed; mapping retained");
          continue;
        }
        if (pendingTagRemoval.stage == TagRemovalStage::ClearingPayload) {
          pendingTagOperation = PendingTagOperation::None;
          pendingTagRemoval = {};
          FS_LOGE(services::LogComponent::App,
                  "Tag assignment removal partial mapping_removed=true payload_cleared=false reason=clear_or_verify_failed");
          rtos::UiCommand hide{};
          hide.type = rtos::UiCommandType::HideProgress;
          sendUiCommand(ctx, hide,
                        "AppTask: removal error progress close overflow");
          sendOverlay(
              ctx, rtos::UiCommandType::ShowDialog,
              rtos::UiOverlayKind::Error, event.requestId,
              "Zuordnung teilweise entfernt",
              "Die Tag-Zuordnung wurde entfernt.\nDie FilamentStation-Daten konnten nicht vom Tag entfernt werden.");
          continue;
        }
        pendingTagOperation = PendingTagOperation::None;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide, "AppTask: NFC error progress close overflow");
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "NFC-Vorgang fehlgeschlagen", event.text);
      }
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      status.spoolId = event.spoolId;
      std::snprintf(status.title, sizeof(status.title), "NFC");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status, "AppTask: NFC status UI queue overflow");
      if (event.type == rtos::AppEventType::NfcInitialized) {
        std::snprintf(bootNfcStatus, sizeof(bootNfcStatus), "NFC: bereit");
        refreshBootProgress(ctx, event.requestId);
      } else if (event.type == rtos::AppEventType::NfcError &&
                 bootNfcStatus[0] == '\0') {
        std::snprintf(bootNfcStatus, sizeof(bootNfcStatus), "NFC: Fehler");
        refreshBootProgress(ctx, event.requestId);
      }
    } else if (event.type == rtos::AppEventType::WifiStationConnected ||
               event.type == rtos::AppEventType::WifiGotIp ||
               event.type == rtos::AppEventType::WifiDisconnected ||
               event.type == rtos::AppEventType::WifiLostIp ||
               event.type == rtos::AppEventType::WifiConfigPortalStarted ||
               event.type == rtos::AppEventType::WifiConfigPortalStopped ||
               event.type == rtos::AppEventType::WifiConfigPortalTimedOut ||
               event.type == rtos::AppEventType::WifiCredentialsCleared) {
      rtos::UiCommand networkStatus{};
      networkStatus.type = rtos::UiCommandType::UpdateNetworkStatus;
      networkStatus.requestId = event.requestId;
      networkStatus.value = event.value;
      std::snprintf(networkStatus.title, sizeof(networkStatus.title), "%s",
                    event.networkSsid);
      std::snprintf(networkStatus.text, sizeof(networkStatus.text), "%s",
                    event.networkIp);
      if (event.type == rtos::AppEventType::WifiGotIp) {
        networkStatus.networkState = rtos::UiNetworkState::Online;
      } else if (event.type == rtos::AppEventType::WifiStationConnected) {
        networkStatus.networkState = rtos::UiNetworkState::Connecting;
      } else if (event.type == rtos::AppEventType::WifiConfigPortalStarted) {
        networkStatus.networkState = rtos::UiNetworkState::PortalActive;
      } else if (event.type == rtos::AppEventType::WifiCredentialsCleared) {
        networkStatus.networkState = rtos::UiNetworkState::CredentialsCleared;
      } else {
        networkStatus.networkState = rtos::UiNetworkState::Offline;
      }
      sendUiCommand(ctx, networkStatus,
                    "AppTask: network details UI queue overflow");

      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "WLAN");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status, "AppTask: WiFi status UI queue overflow");

      if (event.type == rtos::AppEventType::WifiGotIp) {
        // Retry in case WiFi came up after bambu.json finished loading
        // (Connect is idempotent, see connectAllEnabledPrinters).
        connectAllEnabledPrinters(ctx);
        retrySpoolmanHealthCheckIfNeeded(ctx);
      }

      if (event.type == rtos::AppEventType::WifiDisconnected ||
          event.type == rtos::AppEventType::WifiLostIp ||
          event.type == rtos::AppEventType::WifiCredentialsCleared) {
        xEventGroupClearBits(ctx.systemEventGroup,
                             rtos::EVENT_SPOOLMAN_READY |
                                 rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
        publishSpoolmanAppState(ctx, event.requestId);
      }

      if (event.type == rtos::AppEventType::WifiCredentialsCleared) {
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide,
                      "AppTask: WiFi reset progress close overflow");
        pendingOverlay = rtos::UiOverlayKind::None;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, event.requestId,
                    "WLAN-Zugangsdaten gel\xC3\xB6scht",
                    "Das Ger\xC3\xA4t ist nicht mehr mit dem bisherigen WLAN verbunden. Verwenden Sie Neu konfigurieren, um ein WLAN auszuw\xC3\xA4hlen.");
        continue;
      }

      if (event.type == rtos::AppEventType::WifiConfigPortalStarted) {
        wifiPortalRequested = false;
        wifiPortalActive = true;
        wifiPortalRequestId = event.requestId;
        if (event.requestId != 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                      rtos::UiOverlayKind::ConnectionProgress,
                      event.requestId, "WLAN konfigurieren", event.text);
        }
        continue;
      }

      if (event.type == rtos::AppEventType::WifiStationConnected ||
          ((event.type == rtos::AppEventType::WifiDisconnected ||
            event.type == rtos::AppEventType::WifiLostIp) &&
           wifiPortalActive)) {
        continue;
      }

      const bool wasInteractivePortal =
          wifiPortalRequestId != 0 || event.requestId != 0;
      const std::uint32_t resultRequestId =
          event.requestId != 0 ? event.requestId : wifiPortalRequestId;
      wifiPortalRequested = false;
      wifiPortalActive = false;
      wifiPortalRequestId = 0;
      if (!wasInteractivePortal) continue;

      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: WiFi progress close overflow");
      pendingOverlay = rtos::UiOverlayKind::None;
      if (event.type == rtos::AppEventType::WifiGotIp) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, resultRequestId,
                    "WLAN verbunden", event.text);
      } else if (event.type ==
                 rtos::AppEventType::WifiConfigPortalTimedOut) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, resultRequestId,
                    "WLAN-Portal beendet", event.text);
      } else if (event.type ==
                 rtos::AppEventType::WifiConfigPortalStopped) {
        rtos::UiCommand toast{};
        toast.type = rtos::UiCommandType::ShowToast;
        toast.requestId = resultRequestId;
        std::snprintf(toast.text, sizeof(toast.text),
                      "WLAN-Konfiguration abgebrochen");
        sendUiCommand(ctx, toast, "AppTask: WiFi cancel UI queue overflow");
      } else {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, resultRequestId,
                    "WLAN-Verbindung fehlgeschlagen", event.text);
      }
    } else if (event.type == rtos::AppEventType::SpoolmanWeightUpdated) {
      const bool advanced = weightUpdate.advanced;
      weightUpdate = {};
      quickWeight.lastMeasurementGrams = scaleWeightGrams();
      quickWeight.lastMeasurementSpoolId = event.spoolId;
      quickWeight.hasLastMeasurement = true;
      if (advanced) advancedWeight.committed = true;
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: weight progress close overflow");
      stagingSpoolId = event.spoolId;
      rtos::UiCommand staging{};
      staging.type = rtos::UiCommandType::UpdateStaging;
      staging.requestId = event.requestId;
      staging.spoolId = event.spoolId;
      staging.weightUpdate = event.weightUpdate;
      staging.spoolColorCount = event.spoolColorCount;
      for (std::uint8_t color = 0; color < event.spoolColorCount; ++color)
        std::snprintf(staging.spoolColorHex[color],
                      sizeof(staging.spoolColorHex[color]), "%s",
                      event.spoolColorHex[color]);
      std::snprintf(staging.text, sizeof(staging.text), "%s", event.text);
      sendUiCommand(ctx, staging, "AppTask: staging weight update overflow");
      char result[144]{};
      std::snprintf(result, sizeof(result),
                    "Spule #%lu\nRestgewicht: %.1f g\nSpoolman-Daten wurden neu geladen.",
                    static_cast<unsigned long>(event.spoolId),
                    static_cast<double>(event.weightUpdate.remainingWeightGrams));
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  advanced ? rtos::UiOverlayKind::AdvancedWeightResult
                           : rtos::UiOverlayKind::Success,
                  event.requestId,
                  advanced ? "Erweitertes Wiegen gespeichert"
                           : "Messung gespeichert",
                  result);
    } else if (event.type == rtos::AppEventType::SpoolmanImportCompleted) {
      pendingTagSpoolId = event.spoolId;
      currentTag.definition.hasSpoolId = event.spoolId != 0;
      currentTag.definition.spoolId = event.spoolId;
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: import progress close overflow");
      rtos::UiCommand result{};
      result.type = rtos::UiCommandType::ShowScreen;
      result.screenId = rtos::UiScreenId::TagResult;
      result.requestId = event.requestId;
      result.spoolId = event.spoolId;
      std::snprintf(result.text, sizeof(result.text), "%s", event.text);
      previousScreen = currentScreen;
      currentScreen = result.screenId;
      sendUiCommand(ctx, result, "AppTask: import result UI overflow");
    } else if (event.type == rtos::AppEventType::SpoolmanResponse &&
               event.requestId >= kTraySpoolDetailsRequestIdBase &&
               event.requestId < kTraySpoolDetailsRequestIdBase +
                                      kMaximumTraySpoolDetailsEntries) {
      // Response to resolveTraySpoolDetails()'s two-step fetch
      // (LoadSpool -> LoadFilament, see TraySpoolDetailsStage) -- store the
      // result and re-sync Home so the tray card picks it up without
      // waiting for the next unrelated report (Nutzerwunsch 2026-08-24).
      const std::size_t index = event.requestId - kTraySpoolDetailsRequestIdBase;
      TraySpoolDetailsEntry& entry = traySpoolDetails[index];
      if (entry.stage == TraySpoolDetailsStage::LoadingSpool) {
        if (event.value >= 0 && event.spool.id == entry.spoolId) {
          entry.remainingWeightGrams = event.spool.remainingWeightGrams;
          if (event.spool.filamentId != 0 &&
              requestFilamentDetails(ctx, event.requestId,
                                     event.spool.filamentId)) {
            entry.stage = TraySpoolDetailsStage::LoadingFilament;
          } else {
            // No filament id, or the follow-up request couldn't be
            // enqueued -- show the weight we do have rather than nothing.
            entry.stage = TraySpoolDetailsStage::Loaded;
            if (models::isValidPrinterId(printerCollection.activePrinterId))
              syncAmsToUi(ctx, printerCollection.activePrinterId);
          }
        } else {
          entry = TraySpoolDetailsEntry{};
        }
      } else if (entry.stage == TraySpoolDetailsStage::LoadingFilament &&
                 event.filament.id != 0) {
        // event.filament.id != 0 excludes loadSpools()'s trailing "N Spulen
        // gefunden" completion marker (see docs/bambu-protocol.md) -- it
        // reuses this same requestId (LoadSpool -> LoadFilament chain both
        // key off event.requestId) and, for a single-spool LoadSpool, is
        // enqueued immediately after the real spool response, arriving here
        // well before the actual (HTTP-round-trip-bound) filament response.
        // Without this guard it was mistaken for "filament fetch done, no
        // data", prematurely marking the entry Loaded with kFactorValid
        // still false -- the real response then arrived to a stage that no
        // longer matched anything and was silently dropped (bug found via
        // Nutzer-Report 2026-08-24, staging card showed the same symptom).
        entry.kFactorValid = event.filament.bambuKFactorValid;
        entry.kFactor = event.filament.bambuKFactor;
        // Weight is already known from the first step regardless of
        // whether the filament fetch itself succeeded -- show what we
        // have instead of discarding it over a missing K-factor.
        entry.stage = TraySpoolDetailsStage::Loaded;
        if (models::isValidPrinterId(printerCollection.activePrinterId))
          syncAmsToUi(ctx, printerCollection.activePrinterId);
      }
    } else if (event.type == rtos::AppEventType::SpoolmanResponse &&
               event.requestId == kLegacyMigrationLoadSpoolRequestId &&
               legacyMigrationStage == LegacyMigrationStage::LoadingTarget) {
      if (event.value < 0) {
        if (event.spoolId == 0)
          finishLegacyMigrationEntry(ctx, false, "target_spool_not_found");
        continue;
      }
      const auto& mapping = legacyMappingFiles[legacyMigrationFileIndex]
                                .mappings[legacyMigrationEntryIndex];
      if (event.spool.id != mapping.spoolId) {
        finishLegacyMigrationEntry(ctx, false, "target_spool_mismatch");
      } else if (services::legacyMigrationDecision(
                     event.spool.extraTagValid, event.spool.extraTag,
                     legacyMigrationIdentity.value) ==
                 services::LegacyMigrationDecision::SetTarget) {
        legacyMigrationStage = LegacyMigrationStage::SettingTarget;
        if (!sendLegacySpoolmanCommand(
                ctx, rtos::SpoolmanCommandType::SetSpoolTag,
                kLegacyMigrationSetTagRequestId, mapping.spoolId))
          finishLegacyMigrationEntry(ctx, false, "spoolman_queue_full");
      } else if (services::legacyMigrationDecision(
                     event.spool.extraTagValid, event.spool.extraTag,
                     legacyMigrationIdentity.value) ==
                 services::LegacyMigrationDecision::AlreadyMigrated) {
        finishLegacyMigrationEntry(ctx, true, "already_assigned");
      } else {
        finishLegacyMigrationEntry(
            ctx, false, event.spool.extraTagValid
                            ? "target_spool_has_other_tag"
                            : "target_spool_tag_field_invalid");
      }
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagLookup &&
               event.requestId == kLegacyMigrationLookupRequestId &&
               legacyMigrationStage == LegacyMigrationStage::LookingUp) {
      const auto status = static_cast<services::TagLookupStatus>(event.value);
      const auto& mapping = legacyMappingFiles[legacyMigrationFileIndex]
                                .mappings[legacyMigrationEntryIndex];
      if (status == services::TagLookupStatus::Found) {
        finishLegacyMigrationEntry(
            ctx, event.spoolId == mapping.spoolId,
            event.spoolId == mapping.spoolId ? "already_assigned"
                                             : "tag_assigned_elsewhere");
      } else if (status == services::TagLookupStatus::NotFound) {
        legacyMigrationStage = LegacyMigrationStage::LoadingTarget;
        if (!sendLegacySpoolmanCommand(
                ctx, rtos::SpoolmanCommandType::LoadSpool,
                kLegacyMigrationLoadSpoolRequestId, mapping.spoolId))
          finishLegacyMigrationEntry(ctx, false, "spoolman_queue_full");
      } else {
        finishLegacyMigrationEntry(ctx, false, "tag_lookup_failed");
      }
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagDuplicate &&
               event.requestId == kLegacyMigrationLookupRequestId &&
               legacyMigrationStage == LegacyMigrationStage::LookingUp) {
      finishLegacyMigrationEntry(ctx, false,
                                 "tag_assigned_to_multiple_spools");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagUpdated &&
               event.requestId == kLegacyMigrationSetTagRequestId &&
               legacyMigrationStage == LegacyMigrationStage::SettingTarget) {
      finishLegacyMigrationEntry(ctx, true, "assigned");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagLookup &&
               pendingTagResolution.active &&
               event.requestId == pendingTagResolution.requestId) {
      const auto pending = pendingTagResolution;
      pendingTagResolution = {};
      const bool sameTag =
          tagPresent && currentTag.uidLength == pending.uidLength &&
          std::memcmp(currentTag.uid, pending.uid.data(), pending.uidLength) == 0 &&
          currentTag.identity.source == pending.identity.source &&
          std::strcmp(currentTag.identity.value, pending.identity.value) == 0;
      if (!sameTag) continue;
      const auto status = static_cast<services::TagLookupStatus>(event.value);
      resolvedTagIdentity = pending.identity;
      resolvedTagSpoolId =
          status == services::TagLookupStatus::Found ? event.spoolId : 0;
      rtos::AppEvent replay{};
      replay.type = rtos::AppEventType::NfcTagRead;
      replay.requestId = event.requestId;
      replay.tagReadResult = currentTag;
      if (xQueueSend(ctx.appEventQueue, &replay, pdMS_TO_TICKS(1000)) != pdPASS)
        FS_LOGW(services::LogComponent::App,
                "Event enqueue failed queue=app_event op=replay_tag_after_spoolman_lookup");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagDuplicate &&
               pendingTagResolution.active &&
               event.requestId == pendingTagResolution.requestId) {
      pendingTagResolution = {};
      resolvedTagIdentity = {};
      resolvedTagSpoolId = 0;
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Error, event.requestId,
                  "Mehrdeutige Tag-Zuordnung",
                  "Diese Tag-ID ist mehreren Spulen in Spoolman zugeordnet. Bitte die Spoolman-Daten korrigieren.");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanResponse &&
               pendingSlotAssignment.stage ==
                   SlotAssignmentStage::LoadingSpool &&
               event.requestId == pendingSlotAssignment.requestId) {
      if (event.value < 0 || event.spool.id == 0) {
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide,
                      "AppTask: slot assignment progress close overflow");
        pendingSlotAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Slot nicht konfiguriert",
                    "Spulendaten konnten nicht geladen werden.");
        continue;
      }
      // Material/color aus der aufgeloesten Spule; Bambu erwartet
      // trayColorHex als RRGGBB(AA)-Hexstring, colorHex[0] liefert genau
      // das. Fuer die spaetere LoadingFilament-Antwort gemerkt (siehe
      // sendPendingSlotAssignTray()) -- bambu_temp_min/bambu_temp_max sind
      // eine Spoolman *Filament*-Eigenschaft (Nutzerhinweis 2026-08-24),
      // dafuer ein eigener Request statt sich auf das verschachtelte
      // filament-Objekt dieser Spool-Antwort zu verlassen.
      pendingSlotAssignment.spoolId = event.spool.id;
      std::snprintf(pendingSlotAssignment.trayType,
                    sizeof(pendingSlotAssignment.trayType), "%s",
                    event.spool.material);
      std::snprintf(pendingSlotAssignment.trayColorHex,
                    sizeof(pendingSlotAssignment.trayColorHex), "%s",
                    event.spool.colorCount > 0 ? event.spool.colorHex[0] : "");
      if (event.spool.filamentId != 0 &&
          requestFilamentDetails(ctx, event.requestId,
                                 event.spool.filamentId)) {
        pendingSlotAssignment.stage = SlotAssignmentStage::LoadingFilament;
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, event.requestId,
                    "Slot konfigurieren", "Filamentdaten werden geladen.");
        continue;
      }
      // Keine Filament-ID (sollte normalerweise nicht vorkommen) oder der
      // Folge-Request konnte nicht gesendet werden -- ohne Temperatur
      // fortfahren statt die ganze Zuordnung abzubrechen (gleiche
      // Nutzerfreundlichkeit wie bei fehlenden/ungueltigen bambu_temp_min/
      // bambu_temp_max-Werten, siehe LoadingFilament-Behandlung unten).
      FS_LOGD(services::LogComponent::App,
              "LoadFilament skipped request_id=%lu spool_filament_id=%lu -- "
              "proceeding without temperature",
              static_cast<unsigned long>(event.requestId),
              static_cast<unsigned long>(event.spool.filamentId));
      pendingSlotAssignment.tempFieldsMissing = true;
      {
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide,
                      "AppTask: slot assignment progress close overflow");
      }
      if (!sendPendingSlotAssignTray(ctx, event.requestId, 0, 0)) {
        pendingSlotAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Slot nicht konfiguriert",
                    "Der Auftrag konnte nicht an den Drucker gesendet werden.");
        continue;
      }
      pendingSlotAssignment.stage = SlotAssignmentStage::WritingSlot;
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::BambuConnection, event.requestId,
                  "Slot konfigurieren",
                  "Slotdaten werden an den Drucker gesendet.");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanResponse &&
               pendingSlotAssignment.stage ==
                   SlotAssignmentStage::LoadingFilament &&
               event.requestId == pendingSlotAssignment.requestId &&
               event.filament.id != 0) {
      // event.filament.id != 0 excludes loadSpools()'s trailing "N Spulen
      // gefunden" completion marker (see docs/bambu-protocol.md): the
      // preceding LoadSpool request (requestStagingSpool()) and this
      // LoadFilament follow-up both key off event.requestId, and that
      // marker -- always sent after a LoadSpool response, empty spool/
      // filament payload, value=-1 -- reaches this queue well before the
      // real (HTTP-round-trip-bound) filament response. Without this guard
      // it was mistaken for "filament fetch done, no data", sending the
      // AssignTray command with nozzle_temp_min/max=0 immediately -- the
      // real response then arrived to a stage that had already moved to
      // WritingSlot and was silently dropped (root cause of the
      // nozzle_temp_min=0/nozzle_temp_max=0 hardware finding, 2026-08-24).
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide,
                    "AppTask: slot assignment progress close overflow");
      // bambu_temp_min/bambu_temp_max bleiben 0, wenn sie fehlen/ungueltig
      // sind oder der Fetch selbst fehlschlug -- der Nutzer wird darauf per
      // Hinweis im Ergebnisdialog aufmerksam gemacht (siehe die BambuUpdate/
      // BambuError-Behandlung fuer SlotAssignmentStage::WritingSlot), es
      // wird keine Temperatur erfunden.
      std::uint16_t nozzleTempMinC = 0;
      std::uint16_t nozzleTempMaxC = 0;
      if (event.value >= 0 && event.filament.bambuTempFieldsValid) {
        nozzleTempMinC = event.filament.bambuTempMinC;
        nozzleTempMaxC = event.filament.bambuTempMaxC;
        pendingSlotAssignment.tempFieldsMissing = false;
      } else {
        pendingSlotAssignment.tempFieldsMissing = true;
      }
      if (!sendPendingSlotAssignTray(ctx, event.requestId, nozzleTempMinC,
                                     nozzleTempMaxC)) {
        pendingSlotAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Slot nicht konfiguriert",
                    "Der Auftrag konnte nicht an den Drucker gesendet werden.");
        continue;
      }
      pendingSlotAssignment.stage = SlotAssignmentStage::WritingSlot;
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::BambuConnection, event.requestId,
                  "Slot konfigurieren",
                  "Slotdaten werden an den Drucker gesendet.");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanResponse) {
      if (pendingStagingSpoolRequestId != 0 &&
          event.requestId == pendingStagingSpoolRequestId) {
        if (event.value >= 0 && event.spool.id != 0) {
          pendingStagingSpoolRequestId = 0;
          rtos::UiCommand hide{};
          hide.type = rtos::UiCommandType::HideProgress;
          sendUiCommand(ctx, hide, "AppTask: staging load close overflow");
          // emptySpoolWeightGrams/K-Faktor sind Spoolman *Filament*-
          // Eigenschaften (Nutzerhinweis 2026-08-24) -- das eingebettete
          // filament-Objekt dieser Spool-Antwort ist dafuer nicht
          // zuverlaessig genug (dasselbe Problem wie bei bambu_temp_min/
          // bambu_temp_max), daher ein eigener Folge-Request statt K-Faktor
          // wie zuvor als Mockdaten anzuzeigen.
          if (event.spool.filamentId != 0 &&
              requestFilamentDetails(ctx, event.requestId,
                                     event.spool.filamentId)) {
            pendingStagingFilamentLoad.active = true;
            pendingStagingFilamentLoad.requestId = event.requestId;
            pendingStagingFilamentLoad.spool = event.spool;
          } else {
            sendStagingUpdate(ctx, event.requestId, event.spool,
                              event.spool.emptyWeightGrams, false, 0.0F);
          }
        }
        continue;
      }
      if (pendingStagingFilamentLoad.active &&
          event.requestId == pendingStagingFilamentLoad.requestId &&
          event.filament.id != 0) {
        // event.filament.id != 0 excludes loadSpools()'s trailing "N Spulen
        // gefunden" completion marker (see docs/bambu-protocol.md): the
        // preceding LoadSpool request and this LoadFilament follow-up both
        // key off event.requestId, and that marker -- always sent right
        // after a LoadSpool response, empty spool/filament payload,
        // value=-1 -- reaches this queue well before the real (HTTP-round-
        // trip-bound) filament response. Without this guard it was
        // mistaken for "filament fetch done, no data", clearing
        // pendingStagingFilamentLoad with kFactorValid still false -- the
        // real response then arrived to a cleared state and was silently
        // dropped (root cause of the "K-Faktor wird geladen, aber nicht
        // angezeigt" hardware finding, 2026-08-24).
        const models::SpoolmanSpool spool = pendingStagingFilamentLoad.spool;
        pendingStagingFilamentLoad = {};
        float emptyWeightGrams = spool.emptyWeightGrams;
        if (event.filament.emptySpoolWeightGrams > 0.0F)
          emptyWeightGrams = event.filament.emptySpoolWeightGrams;
        sendStagingUpdate(ctx, event.requestId, spool, emptyWeightGrams,
                          event.filament.bambuKFactorValid,
                          event.filament.bambuKFactor);
        continue;
      }
      rtos::UiCommand picker{};
      picker.type = rtos::UiCommandType::UpdateSpoolPicker;
      picker.requestId = event.requestId;
      picker.value = event.value;
      picker.spoolId = event.spoolId;
      picker.spoolColorCount = event.spoolColorCount;
      for (std::uint8_t color = 0; color < event.spoolColorCount; ++color)
        std::snprintf(picker.spoolColorHex[color],
                      sizeof(picker.spoolColorHex[color]), "%s",
                      event.spoolColorHex[color]);
      std::snprintf(picker.text, sizeof(picker.text), "%s", event.text);
      sendUiCommand(ctx, picker, "AppTask: Spoolman picker result overflow");
    } else if (event.type == rtos::AppEventType::SpoolmanTagDuplicate) {
      const bool nativeCheck =
          pendingNativeConsistency.active &&
          event.requestId == pendingNativeConsistency.requestId &&
          event.tagIdentity.source == pendingNativeConsistency.identity.source &&
          std::strcmp(event.tagIdentity.value,
                      pendingNativeConsistency.identity.value) == 0;
      const bool removalCheck =
          pendingTagRemoval.stage == TagRemovalStage::LookingUp &&
          event.requestId == pendingTagRemoval.requestId &&
          event.tagIdentity.source == pendingTagRemoval.identity.source &&
          std::strcmp(event.tagIdentity.value,
                      pendingTagRemoval.identity.value) == 0;
      const bool assignmentCheck =
          pendingTagAssignment.stage == TagAssignmentStage::LookingUp &&
          event.requestId == pendingTagAssignment.requestId &&
          event.tagIdentity.source == pendingTagAssignment.identity.source &&
          std::strcmp(event.tagIdentity.value,
                      pendingTagAssignment.identity.value) == 0;
      if (!nativeCheck && !removalCheck && !assignmentCheck) {
        FS_LOGW(services::LogComponent::App,
                "Stale duplicate tag event ignored tag=%s matches=%ld",
                event.tagIdentity.value, static_cast<long>(event.value));
        continue;
      }
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: duplicate lookup close overflow");
      if (nativeCheck) {
        pendingNativeConsistency = {};
        resolvedTagIdentity = {};
        resolvedTagSpoolId = 0;
        showNativeTagAction(
            ctx, event.requestId, 0,
            "Konflikt: Diese Tag-ID ist mehreren Spulen in Spoolman zugeordnet.");
      }
      if (removalCheck) {
        pendingTagRemoval = {};
        pendingUnlinkConfirmation = false;
      }
      if (assignmentCheck) pendingTagAssignment = {};
      FS_LOGE(services::LogComponent::App,
              "Duplicate tag assignment blocked tag=%s matches=%ld operation=%s",
              event.tagIdentity.value, static_cast<long>(event.value),
              nativeCheck ? "consistency"
                          : removalCheck ? "remove" : "assign");
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Error, event.requestId,
                  "Mehrdeutige Tag-Zuordnung",
                  "Diese Tag-ID ist mehreren Spulen zugeordnet. Es wurde keine Spule ausgew\xC3\xA4hlt und keine Zuordnung ge\xC3\xA4ndert. Bitte die Spoolman-Daten korrigieren.");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagLookup &&
               pendingNativeConsistency.active &&
               event.requestId == pendingNativeConsistency.requestId) {
      const auto check = pendingNativeConsistency;
      pendingNativeConsistency = {};
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: consistency lookup close overflow");
      const bool sameTag =
          tagPresent && currentTag.uidLength == check.uidLength &&
          std::memcmp(currentTag.uid, check.uid.data(), check.uidLength) == 0 &&
          currentTag.identity.source == check.identity.source &&
          std::strcmp(currentTag.identity.value, check.identity.value) == 0;
      if (!sameTag) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Pr\xC3\xBC" "fung abgebrochen",
                    "Der NFC-Tag wurde w\xC3\xA4hrend der Pr\xC3\xBC" "fung entfernt oder ausgetauscht.");
        continue;
      }
      const auto status = static_cast<services::TagLookupStatus>(event.value);
      if (status == services::TagLookupStatus::Duplicate) {
        resolvedTagIdentity = {};
        resolvedTagSpoolId = 0;
        showNativeTagAction(
            ctx, event.requestId, 0,
            "Konflikt: Diese Tag-ID ist mehreren Spulen in Spoolman zugeordnet.");
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Mehrdeutige Tag-Zuordnung",
                    "Diese Tag-ID ist mehreren Spulen zugeordnet. Der NDEF-Payload wird nicht als Zuordnung verwendet.");
        continue;
      }
      if (status == services::TagLookupStatus::NotFound) {
        resolvedTagIdentity = {};
        resolvedTagSpoolId = 0;
        char conflict[160]{};
        std::snprintf(conflict, sizeof(conflict),
                      "Konflikt: NDEF nennt Spule #%lu, in Spoolman fehlt die Zuordnung.",
                      static_cast<unsigned long>(check.payloadSpoolId));
        showNativeTagAction(ctx, event.requestId, 0, conflict);
        FS_LOGW(services::LogComponent::App,
                "Native tag consistency mismatch reason=server_assignment_missing payload_spool_id=%lu tag=%s",
                static_cast<unsigned long>(check.payloadSpoolId),
                check.identity.value);
        continue;
      }
      if (status == services::TagLookupStatus::Found && event.spoolId != 0) {
        resolvedTagIdentity = check.identity;
        resolvedTagSpoolId = event.spoolId;
        pendingTagSpoolId = event.spoolId;
        char state[160]{};
        if (event.spoolId == check.payloadSpoolId) {
          std::snprintf(state, sizeof(state),
                        "Konsistent zu Spule #%lu zugeordnet",
                        static_cast<unsigned long>(event.spoolId));
          FS_LOGI(services::LogComponent::App,
                  "Native tag consistency verified spool_id=%lu tag=%s",
                  static_cast<unsigned long>(event.spoolId),
                  check.identity.value);
        } else {
          std::snprintf(state, sizeof(state),
                        "Konflikt: NDEF #%lu, Spoolman-Zuordnung #%lu. Spoolman ist f\xC3\xBChrend.",
                        static_cast<unsigned long>(check.payloadSpoolId),
                        static_cast<unsigned long>(event.spoolId));
          FS_LOGW(services::LogComponent::App,
                  "Native tag consistency mismatch payload_spool_id=%lu server_spool_id=%lu tag=%s",
                  static_cast<unsigned long>(check.payloadSpoolId),
                  static_cast<unsigned long>(event.spoolId),
                  check.identity.value);
        }
        showNativeTagAction(ctx, event.requestId, event.spoolId, state);
        continue;
      }
      resolvedTagIdentity = {};
      resolvedTagSpoolId = 0;
      showNativeTagAction(ctx, event.requestId, 0,
                          "Spoolman-Zuordnung konnte nicht bestimmt werden.");
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagLookup &&
               pendingTagRemoval.stage == TagRemovalStage::LookingUp &&
               event.requestId == pendingTagRemoval.requestId) {
      const auto status = static_cast<services::TagLookupStatus>(event.value);
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: removal lookup close overflow");
      if (status == services::TagLookupStatus::Duplicate) {
        pendingTagRemoval = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Mehrdeutige Zuordnung",
                    "Diese Tag-ID ist mehreren Spulen zugeordnet. Bitte die Spoolman-Daten korrigieren.");
        continue;
      }
      if (status != services::TagLookupStatus::Found || event.spoolId == 0) {
        pendingTagRemoval = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Keine Zuordnung gefunden",
                    "Dieser Tag ist in Spoolman keiner Spule zugeordnet.");
        continue;
      }
      if (!tagPresent || pendingTagRemoval.tagRemoved ||
          !removalTagMatches(currentTag)) {
        pendingTagRemoval = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Entfernen abgebrochen",
                    "Der Tag wurde w\xC3\xA4hrend der Pr\xC3\xBC" "fung entfernt oder ausgetauscht.");
        continue;
      }
      pendingTagRemoval.spoolId = event.spoolId;
      pendingTagRemoval.stage = TagRemovalStage::AwaitingConfirmation;
      pendingUnlinkConfirmation = true;
      char confirmation[192]{};
      std::snprintf(
          confirmation, sizeof(confirmation),
          pendingTagRemoval.clearPayload
              ? "Die Verbindung zu Spule #%lu wird entfernt.\nDie FilamentStation-Daten werden auch vom Tag entfernt."
              : "Die Verbindung zu Spule #%lu wird entfernt.\nDer originale Taginhalt wird nicht ver\xC3\xA4ndert.",
          static_cast<unsigned long>(event.spoolId));
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Confirmation, event.requestId,
                  "Tag-Zuordnung entfernen?", confirmation);
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagLookup &&
               pendingTagAssignment.stage == TagAssignmentStage::LookingUp &&
               event.requestId == pendingTagAssignment.requestId) {
      const auto status = static_cast<services::TagLookupStatus>(event.value);
      if (status == services::TagLookupStatus::Duplicate) {
        pendingTagAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Mehrdeutige Zuordnung",
                    "Dieselbe Tag-ID ist mehreren Spulen zugeordnet. Bitte den Konflikt in Spoolman beheben.");
        continue;
      }
      if (status == services::TagLookupStatus::Found &&
          event.spoolId == pendingTagAssignment.spoolId) {
        FS_LOGI(services::LogComponent::App,
                "AssignTag idempotent spool_id=%lu tag=%s",
                static_cast<unsigned long>(event.spoolId),
                pendingTagAssignment.identity.value);
        continueAssignmentAfterSpoolmanUpdate(ctx);
        continue;
      }
      if (status == services::TagLookupStatus::Found) {
        pendingTagAssignment.previousSpoolId = event.spoolId;
        pendingTagAssignment.stage =
            TagAssignmentStage::AwaitingReassignmentConfirmation;
        pendingServerReassignmentConfirmation = true;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide, "AppTask: reassignment lookup close overflow");
        char question[192]{};
        std::snprintf(question, sizeof(question),
                      "Dieser Tag ist Spule %lu zugeordnet. Die Zuordnung durch Spule %lu ersetzen?",
                      static_cast<unsigned long>(event.spoolId),
                      static_cast<unsigned long>(pendingTagAssignment.spoolId));
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Confirmation, event.requestId,
                    "Zuordnung ersetzen", question);
        continue;
      }
      pendingTagAssignment.stage = TagAssignmentStage::SettingTarget;
      if (!sendTagAssignmentCommand(ctx, rtos::SpoolmanCommandType::SetSpoolTag,
                                    pendingTagAssignment.spoolId)) {
        pendingTagAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Zuordnung fehlgeschlagen",
                    "Die Spoolman-Aktualisierung konnte nicht gestartet werden.");
      }
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagUpdated &&
               pendingTagRemoval.stage ==
                   TagRemovalStage::ClearingServerAssignment &&
               event.requestId == pendingTagRemoval.requestId) {
      FS_LOGI(services::LogComponent::App,
              "Tag assignment removed from Spoolman spool_id=%lu tag=%s",
              static_cast<unsigned long>(pendingTagRemoval.spoolId),
              pendingTagRemoval.identity.value);
      continueRemovalAfterSpoolmanUpdate(ctx);
      continue;
    } else if (event.type == rtos::AppEventType::SpoolmanTagUpdated &&
               event.requestId == pendingTagAssignment.requestId) {
      if (pendingTagAssignment.stage == TagAssignmentStage::ClearingPrevious) {
        pendingTagAssignment.stage = TagAssignmentStage::SettingTarget;
        if (!sendTagAssignmentCommand(ctx, rtos::SpoolmanCommandType::SetSpoolTag,
                                      pendingTagAssignment.spoolId)) {
          pendingTagAssignment.stage = TagAssignmentStage::RollingBackPrevious;
          if (!sendTagAssignmentCommand(
                  ctx, rtos::SpoolmanCommandType::SetSpoolTag,
                  pendingTagAssignment.previousSpoolId)) {
            pendingTagAssignment = {};
            sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                        rtos::UiOverlayKind::Error, event.requestId,
                        "Zuordnung inkonsistent",
                        "Die vorherige Spoolman-Zuordnung konnte nicht wiederhergestellt werden. Bitte Spoolman pr\xC3\xBC" "fen.");
          }
        }
        continue;
      }
      if (pendingTagAssignment.stage == TagAssignmentStage::SettingTarget) {
        FS_LOGI(services::LogComponent::App,
                "AssignTag Spoolman mapping stored spool_id=%lu tag=%s",
                static_cast<unsigned long>(pendingTagAssignment.spoolId),
                pendingTagAssignment.identity.value);
        continueAssignmentAfterSpoolmanUpdate(ctx);
        continue;
      }
      if (pendingTagAssignment.stage ==
          TagAssignmentStage::RollingBackPrevious) {
        pendingTagAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Zuordnung fehlgeschlagen",
                    "Die neue Zuordnung schlug fehl. Die vorherige Spoolman-Zuordnung wurde wiederhergestellt.");
        continue;
      }
    } else if (event.type == rtos::AppEventType::SpoolmanConnected) {
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: Spoolman progress close overflow");
      if (pendingTagResolution.active &&
          event.requestId == pendingTagResolution.requestId) {
        resolvedTagIdentity = pendingTagResolution.identity;
        resolvedTagSpoolId = 0;
        pendingTagResolution = {};
        FS_LOGW(services::LogComponent::App,
                "Tag resolution unavailable source=spoolman error=%s",
                event.text);
        rtos::AppEvent replay{};
        replay.type = rtos::AppEventType::NfcTagRead;
        replay.requestId = event.requestId;
        replay.tagReadResult = currentTag;
        xQueueSend(ctx.appEventQueue, &replay, pdMS_TO_TICKS(1000));
        continue;
      }
      char serverVersion[32] = "unbekannt";
      const char* version = std::strstr(event.text, "Version ");
      if (version != nullptr) {
        version += 8;
        const char* end = std::strstr(version, " | ");
        const std::size_t length = end != nullptr
                                       ? static_cast<std::size_t>(end - version)
                                       : std::strlen(version);
        std::snprintf(serverVersion, sizeof(serverVersion), "%.*s",
                      static_cast<int>(length), version);
      }
      publishSpoolmanAppState(ctx, event.requestId, serverVersion);
      // The silent boot-time/apply-configuration health check (see
      // SpoolmanTask::ApplyConfiguration) reuses this exact event type so
      // EVENT_SPOOLMAN_READY/EVENT_SPOOLMAN_TAG_FIELD_READY get set without
      // requiring the user to press "Verbindung testen" after every
      // restart -- but it must not pop an unprompted "Spoolman verbunden"
      // dialog on every startup the way a manual test intentionally does.
      if (event.requestId != kSpoolmanLoadRequestId) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, event.requestId,
                    "Spoolman verbunden", event.text);
      }
      tryStartLegacyMigration(ctx);
    } else if (event.type == rtos::AppEventType::SpoolmanTagFieldReady) {
      publishSpoolmanAppState(ctx, event.requestId);
    } else if (event.type == rtos::AppEventType::SpoolmanError) {
      if ((event.requestId == kLegacyMigrationLookupRequestId ||
           event.requestId == kLegacyMigrationLoadSpoolRequestId ||
           event.requestId == kLegacyMigrationSetTagRequestId) &&
          legacyMigrationStage != LegacyMigrationStage::Complete) {
        FS_LOGW(services::LogComponent::App,
                "Legacy mapping migration request failed request_id=%lu error=%s",
                static_cast<unsigned long>(event.requestId), event.text);
        finishLegacyMigrationEntry(
            ctx, false,
            event.text[0] != '\0' ? event.text : "spoolman_request_failed");
        continue;
      }
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: Spoolman progress close overflow");
      if (pendingNativeConsistency.active &&
          event.requestId == pendingNativeConsistency.requestId) {
        pendingNativeConsistency = {};
        resolvedTagIdentity = {};
        resolvedTagSpoolId = 0;
        if (tagPresent) {
          showNativeTagAction(
              ctx, event.requestId, 0,
              "Spoolman-Pr\xC3\xBC" "fung fehlgeschlagen. NDEF wird nicht als Zuordnung verwendet.");
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Zuordnung nicht pr\xC3\xBC" "fbar",
                    event.text[0] != '\0' ? event.text
                                           : "Spoolman-Anfrage fehlgeschlagen.");
        continue;
      }
      if ((pendingTagRemoval.stage == TagRemovalStage::LookingUp ||
           pendingTagRemoval.stage ==
               TagRemovalStage::ClearingServerAssignment) &&
          event.requestId == pendingTagRemoval.requestId) {
        pendingTagRemoval = {};
        pendingUnlinkConfirmation = false;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Entfernen fehlgeschlagen",
                    event.text[0] != '\0' ? event.text
                                           : "Spoolman-Anfrage fehlgeschlagen.");
        continue;
      }
      if (pendingTagAssignment.stage != TagAssignmentStage::None &&
          pendingTagAssignment.stage != TagAssignmentStage::SelectingSpool &&
          pendingTagAssignment.stage != TagAssignmentStage::WritingPayload &&
          event.requestId == pendingTagAssignment.requestId) {
        if (pendingTagAssignment.stage == TagAssignmentStage::SettingTarget &&
            pendingTagAssignment.previousSpoolId != 0) {
          pendingTagAssignment.stage = TagAssignmentStage::RollingBackPrevious;
          if (sendTagAssignmentCommand(ctx,
                                       rtos::SpoolmanCommandType::SetSpoolTag,
                                       pendingTagAssignment.previousSpoolId)) {
            FS_LOGW(services::LogComponent::App,
                    "AssignTag target update failed; rollback requested old_spool_id=%lu",
                    static_cast<unsigned long>(pendingTagAssignment.previousSpoolId));
            continue;
          }
          pendingTagAssignment = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, event.requestId,
                      "Zuordnung inkonsistent",
                      "Neue Zuordnung und Wiederherstellung konnten nicht abgeschlossen werden. Bitte Spoolman pr\xC3\xBC" "fen.");
          continue;
        }
        const bool rollbackFailed =
            pendingTagAssignment.stage == TagAssignmentStage::RollingBackPrevious;
        pendingTagAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    rollbackFailed ? "Zuordnung inkonsistent"
                                   : "Zuordnung fehlgeschlagen",
                    rollbackFailed
                        ? "Die vorherige Spoolman-Zuordnung konnte nicht wiederhergestellt werden. Bitte Spoolman pr\xC3\xBC" "fen."
                        : (event.text[0] != '\0' ? event.text
                                                 : "Spoolman-Anfrage fehlgeschlagen."));
        continue;
      }
      if (weightUpdate.active && event.requestId == weightUpdate.requestId) {
        const bool permanentMissingSpool =
            std::strstr(event.text, "HTTP 404") != nullptr;
        weightUpdate = {};
        char message[192]{};
        std::snprintf(message, sizeof(message), "%s\n%s",
                      event.text[0] != '\0'
                          ? event.text
                          : "Unbekannter Spoolman-Fehler",
                      permanentMissingSpool
                          ? "Bitte eine vorhandene Spule neu ausw\xC3\xA4hlen."
                          : "Bitte den Wiegevorgang manuell erneut ausf\xC3\xBChren.");
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    permanentMissingSpool ? "Spule nicht gefunden"
                                          : "Gewicht nicht gespeichert",
                    message);
        continue;
      }
      if (pendingStagingSpoolRequestId != 0 &&
          event.requestId == pendingStagingSpoolRequestId) {
        pendingStagingSpoolRequestId = 0;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Spule konnte nicht geladen werden", event.text);
        continue;
      }
      if (pendingStagingFilamentLoad.active &&
          event.requestId == pendingStagingFilamentLoad.requestId) {
        // Filament fetch failed (network hiccup etc.) -- spool data is
        // already known from the LoadSpool step, so show the staged spool
        // anyway without empty-weight/K-Faktor rather than abandoning the
        // staging selection (same graceful degradation as the AssignTray
        // LoadingFilament error path above).
        const models::SpoolmanSpool spool = pendingStagingFilamentLoad.spool;
        pendingStagingFilamentLoad = {};
        sendStagingUpdate(ctx, event.requestId, spool,
                          spool.emptyWeightGrams, false, 0.0F);
        continue;
      }
      if (pendingSlotAssignment.stage == SlotAssignmentStage::LoadingSpool &&
          event.requestId == pendingSlotAssignment.requestId) {
        pendingSlotAssignment = {};
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Slot nicht konfiguriert",
                    event.text[0] != '\0'
                        ? event.text
                        : "Spulendaten konnten nicht geladen werden.");
        continue;
      }
      if (pendingSlotAssignment.stage == SlotAssignmentStage::LoadingFilament &&
          event.requestId == pendingSlotAssignment.requestId) {
        // Filament fetch failed (network hiccup etc.) -- material/color are
        // already known from the LoadingSpool step, so proceed without a
        // temperature range instead of abandoning an otherwise-successful
        // assignment (same graceful degradation as missing/invalid
        // bambu_temp_min/bambu_temp_max, see the SpoolmanResponse handler).
        pendingSlotAssignment.tempFieldsMissing = true;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide,
                      "AppTask: slot assignment progress close overflow");
        if (!sendPendingSlotAssignTray(ctx, event.requestId, 0, 0)) {
          pendingSlotAssignment = {};
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, event.requestId,
                      "Slot nicht konfiguriert",
                      "Der Auftrag konnte nicht an den Drucker gesendet werden.");
          continue;
        }
        pendingSlotAssignment.stage = SlotAssignmentStage::WritingSlot;
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::BambuConnection, event.requestId,
                    "Slot konfigurieren",
                    "Slotdaten werden an den Drucker gesendet.");
        continue;
      }
      if (event.requestId >= kTraySpoolDetailsRequestIdBase &&
          event.requestId < kTraySpoolDetailsRequestIdBase +
                                 kMaximumTraySpoolDetailsEntries) {
        // resolveTraySpoolDetails()'s LoadSpool or LoadFilament step failed.
        const std::size_t index =
            event.requestId - kTraySpoolDetailsRequestIdBase;
        TraySpoolDetailsEntry& entry = traySpoolDetails[index];
        if (entry.stage == TraySpoolDetailsStage::LoadingFilament) {
          // Weight is already known from the completed LoadingSpool step --
          // show it without a K-factor rather than discarding it.
          entry.stage = TraySpoolDetailsStage::Loaded;
          if (models::isValidPrinterId(printerCollection.activePrinterId))
            syncAmsToUi(ctx, printerCollection.activePrinterId);
        } else {
          entry = TraySpoolDetailsEntry{};
        }
        continue;
      }
      if (currentScreen == rtos::UiScreenId::TagDefinitionImport ||
          currentScreen == rtos::UiScreenId::TagLegacy) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Import fehlgeschlagen", event.text);
        continue;
      }
      publishSpoolmanAppState(ctx, event.requestId);
      // Silent boot-time/apply-configuration health check (see the matching
      // guard in the SpoolmanConnected branch above) -- a failed automatic
      // check must not pop an error dialog on every offline startup; the
      // status bar/Settings screen already reflect "Spoolman: offline" via
      // publishSpoolmanAppState().
      if (event.requestId != kSpoolmanLoadRequestId) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Spoolman nicht erreichbar", event.text);
      }
    } else if (event.type == rtos::AppEventType::BambuAssignProgress) {
      // Countdown feedback for the "Wird an den Drucker uebertragen"
      // progress overlay while AppTask waits for BambuTask's telemetry
      // confirmation (see PendingTrayAssignment in BambuTask.cpp). Ignored
      // if the user has since left/cancelled that wait (dialog closed,
      // stage no longer WritingSlot) or this is a stale event for an
      // already-superseded request.
      if (pendingSlotAssignment.stage == SlotAssignmentStage::WritingSlot &&
          event.requestId == pendingSlotAssignment.requestId) {
        const std::int32_t remainingMs = event.value;
        const std::int32_t remainingSeconds = (remainingMs + 999) / 1000;
        rtos::UiCommand progress{};
        progress.type = rtos::UiCommandType::UpdateProgress;
        progress.requestId = event.requestId;
        progress.value = static_cast<std::int32_t>(
            (static_cast<std::int64_t>(remainingMs) * 100) /
            static_cast<std::int64_t>(config::kBambuAssignConfirmTimeoutMs));
        std::snprintf(
            progress.text, sizeof(progress.text),
            "Warte auf Best\xC3\xA4tigung vom Drucker \xE2\x80\x93 noch %ld s",
            static_cast<long>(remainingSeconds));
        sendUiCommand(ctx, progress, "AppTask: assign progress overflow");
      }
    } else if (event.type == rtos::AppEventType::BambuConnected ||
               event.type == rtos::AppEventType::BambuDisconnected ||
               event.type == rtos::AppEventType::BambuUpdate ||
               event.type == rtos::AppEventType::BambuTestResult ||
               event.type == rtos::AppEventType::BambuError) {
      // BambuTestResult carries an empty PrinterState (ephemeral connection
      // test, see BambuTask::handleTestConnection) and must not overwrite a
      // tracked printer's live data.
      if (event.type != rtos::AppEventType::BambuTestResult &&
          models::isValidPrinterId(event.printerId)) {
        models::PrinterState& entry = printerEntry(event.printerId);
        if (event.type == rtos::AppEventType::BambuError) {
          entry.connectionState = models::PrinterConnectionState::Error;
        } else {
          const bool wasActive =
              printerCollection.activePrinterId == event.printerId;
          entry = event.printerState;
          entry.printerId = event.printerId;
          entry.isActive = wasActive;
        }
      }

      // Antwort/Reload/Ergebnis: the AssignTray outcome for a pending slot
      // assignment is shown regardless of which printer is currently in
      // focus (the user is explicitly waiting on this one operation).
      if (pendingSlotAssignment.stage == SlotAssignmentStage::WritingSlot &&
          event.requestId == pendingSlotAssignment.requestId &&
          (event.type == rtos::AppEventType::BambuUpdate ||
           event.type == rtos::AppEventType::BambuError)) {
        const bool success = event.type == rtos::AppEventType::BambuUpdate;
        // ResetSlot/UntagSlot (Phase 9.9) run through this same AssignTray
        // completion path with spoolId == 0 -- distinguish the dialog
        // wording without adding a second pending-state machine.
        const bool wasClearing = pendingSlotAssignment.spoolId == 0;
        const bool tempFieldsMissing = pendingSlotAssignment.tempFieldsMissing;
        // Persist (or drop) the Spoolman association locally now that the
        // printer's own telemetry has confirmed the write -- see
        // models/TraySpoolCache.h and docs/bambu-protocol.md. amsId here is
        // the UI-side 1-based number (same conversion as the AssignTray
        // command above); the material/colorHex captured at this moment
        // become the baseline a later mismatch is checked against.
        if (success) {
          const std::uint8_t amsIndex =
              static_cast<std::uint8_t>(pendingSlotAssignment.amsId - 1U);
          if (wasClearing) {
            models::removeTraySpoolCacheEntry(traySpoolCache,
                                              pendingSlotAssignment.printerId,
                                              amsIndex,
                                              pendingSlotAssignment.trayId);
            persistTraySpoolCache(ctx);
          } else {
            const models::PrinterState* printer = models::findPrinter(
                printerCollection, pendingSlotAssignment.printerId);
            if (printer != nullptr && amsIndex < models::kMaximumAmsPerPrinter &&
                pendingSlotAssignment.trayId < models::kSlotsPerAms) {
              const models::PrinterSlotStateData& slot =
                  printer->amsUnits[amsIndex]
                      .slots[pendingSlotAssignment.trayId];
              models::TraySpoolCacheEntry cacheEntry{};
              cacheEntry.printerId = pendingSlotAssignment.printerId;
              cacheEntry.amsId = amsIndex;
              cacheEntry.trayId = pendingSlotAssignment.trayId;
              cacheEntry.spoolId = pendingSlotAssignment.spoolId;
              std::snprintf(cacheEntry.material, sizeof(cacheEntry.material),
                            "%s", slot.material);
              std::snprintf(cacheEntry.colorHex, sizeof(cacheEntry.colorHex),
                            "%s", slot.colorHex);
              if (models::upsertTraySpoolCacheEntry(traySpoolCache,
                                                    cacheEntry)) {
                persistTraySpoolCache(ctx);
              } else {
                FS_LOGW(services::LogComponent::App,
                        "Tray-Spoolman cache full, association not "
                        "persisted printer_id=%u",
                        static_cast<unsigned>(
                            pendingSlotAssignment.printerId));
              }
            }
          }
        }
        pendingSlotAssignment = {};
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide,
                      "AppTask: slot assignment progress close overflow");
        if (success) {
          // Kein sofortiges RequestStatus/pushall mehr direkt nach
          // AssignTray: das hat eine Statusabfrage ausgeloest, noch bevor
          // der Drucker die neuen AMS-Werte intern fertig verarbeitet und
          // gespeichert hatte, und dadurch vermutlich zum Zuruecksetzen der
          // Zuordnung nach 4-6 Sekunden beigetragen (Nutzer-Diagnose,
          // 2026-08-22). Der Drucker sendet nach einer erfolgreichen
          // Parameteraenderung von sich aus ein Status-Update; die naechste
          // periodische Push-Nachricht (alle paar Sekunden, siehe
          // BambuTask::doConnect-Subscription) aktualisiert Home von
          // alleine, ganz ohne diesen Befehl.
          // Navigate back to Home underneath the confirmation dialog rather
          // than leaving the user stranded on TraySelect/TrayActions after
          // dismissing it -- the slot action is complete, there is nothing
          // more to do on that screen.
          rtos::UiCommand home{};
          home.type = rtos::UiCommandType::ShowScreen;
          home.screenId = rtos::UiScreenId::Home;
          currentScreen = home.screenId;
          previousScreen = home.screenId;
          sendUiCommand(ctx, home,
                        "AppTask: slot assignment home navigation overflow");
        }
        char resultText[192];
        if (success) {
          std::snprintf(
              resultText, sizeof(resultText), "%s",
              wasClearing
                  ? "Der Slot wurde geleert und an den Drucker \xC3\xBC"
                    "bertragen."
                  : "Die Spule wurde dem AMS-Slot zugeordnet und an den "
                    "Drucker \xC3\xBC" "bertragen.");
          // Ohne bambu_temp_min/bambu_temp_max in Spoolman (Filament-Extra-
          // Felder) wurden nur Material/Farbe uebertragen, keine
          // Duesentemperatur -- Hinweis statt erfundener Werte.
          if (!wasClearing && tempFieldsMissing) {
            const std::size_t used = std::strlen(resultText);
            std::snprintf(
                resultText + used, sizeof(resultText) - used,
                "\nHinweis: Keine g\xC3\xBCltige Bambu-Duesentemperatur in "
                "Spoolman (Filament-Extra-Felder bambu_temp_min/"
                "bambu_temp_max fehlen oder sind ung\xC3\xBCltig).");
          }
        } else {
          std::snprintf(resultText, sizeof(resultText), "%s",
                        event.text[0] != '\0'
                            ? event.text
                            : "Der Drucker hat die Slotdaten nicht angenommen.");
        }
        sendOverlay(
            ctx,
            rtos::UiCommandType::ShowDialog,
            success ? rtos::UiOverlayKind::Success
                    : rtos::UiOverlayKind::Error,
            event.requestId,
            success ? (wasClearing ? "Slot zur\xC3\xBC" "ckgesetzt"
                                   : "Slot konfiguriert")
                    : "Slot nicht konfiguriert",
            resultText);
        continue;
      }

      // testen (Phase 8.6): TestConnection is ephemeral and never touches
      // printerCollection (see the BambuTestResult exclusion above); its
      // result is only ever this dialog.
      if (event.type == rtos::AppEventType::BambuTestResult &&
          pendingPrinterTestRequestId != 0 &&
          event.requestId == pendingPrinterTestRequestId) {
        pendingPrinterTestRequestId = 0;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide, "AppTask: printer test progress close overflow");
        const bool success = event.value != 0;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    success ? rtos::UiOverlayKind::Success
                            : rtos::UiOverlayKind::Error,
                    event.requestId, "Bambu-Verbindung", event.text);
        continue;
      }

      if (event.type == rtos::AppEventType::BambuError) {
        // Printer connectivity is routinely absent (LAN-only, not always
        // reachable/configured yet); log for diagnosis instead of an
        // intrusive dialog.
        FS_LOGW(services::LogComponent::App,
                "Bambu request failed printer_id=%u error=\"%s\"",
                static_cast<unsigned>(event.printerId), event.text);
      } else {
        const models::PrinterState* runtimeState =
            models::findPrinter(printerCollection, event.printerId);
        std::uint8_t presentAmsCount = 0;
        std::uint8_t occupiedTotal = 0;
        if (runtimeState != nullptr) {
          for (const auto& ams : runtimeState->amsUnits) {
            if (!ams.present) continue;
            ++presentAmsCount;
            for (const auto& slot : ams.slots)
              if (slot.state == models::PrinterSlotState::Ready) ++occupiedTotal;
          }
        }
        FS_LOGT(services::LogComponent::App,
                "Bambu event received type=%u printer_id=%u "
                "connection_state=%u ams_present=%u trays_occupied=%u "
                "external_state=%u focused=%s",
                static_cast<unsigned>(event.type),
                static_cast<unsigned>(event.printerId),
                runtimeState != nullptr
                    ? static_cast<unsigned>(runtimeState->connectionState)
                    : 0U,
                static_cast<unsigned>(presentAmsCount),
                static_cast<unsigned>(occupiedTotal),
                runtimeState != nullptr
                    ? static_cast<unsigned>(runtimeState->externalSlot.state)
                    : 0U,
                event.printerId == printerCollection.activePrinterId ? "yes"
                                                                     : "no");
      }

      if (event.type != rtos::AppEventType::BambuTestResult)
        syncPrinterEntryToUi(ctx, event.printerId);

      // Stale Responses: printerCollection above is always kept current for
      // whichever printer the event is about, but the visible Header/AMS
      // overview only refreshes if that printer is still the one in focus;
      // otherwise this is a background update about a printer the user has
      // since switched away from.
      if (event.type == rtos::AppEventType::BambuTestResult ||
          event.printerId != printerCollection.activePrinterId) {
        continue;
      }
      if (event.type == rtos::AppEventType::BambuConnected ||
          event.type == rtos::AppEventType::BambuUpdate ||
          event.type == rtos::AppEventType::BambuDisconnected) {
        rtos::UiCommand header{};
        header.type = rtos::UiCommandType::UpdateHeader;
        header.printerId = event.printerId;
        sendUiCommand(ctx, header, "AppTask: Bambu header update overflow");
        syncAmsToUi(ctx, event.printerId);
      }
    } else if (event.type == rtos::AppEventType::UiCommunicationTest) {
      uiStartupReady = true;
      publishSpoolmanAppState(ctx, event.requestId);
      rtos::UiCommand response{};
      response.type = rtos::UiCommandType::CommunicationTestResponse;
      response.requestId = event.requestId;
      std::snprintf(response.title, sizeof(response.title), "RTOS test");
      std::snprintf(response.text, sizeof(response.text), "AppTask acknowledged event");
      if (sendUiCommand(ctx, response,
                        "AppTask: uiCommandQueue timeout/overflow")) {
        FS_LOGD(services::LogComponent::App,
                "Communication test response sent request_id=%lu",
                static_cast<unsigned long>(response.requestId));
      }
      refreshBootProgress(ctx, event.requestId);
      showHomeWhenStartupReady(ctx);
    } else if (event.type == rtos::AppEventType::SdMounted ||
               event.type == rtos::AppEventType::SdRemoved ||
               event.type == rtos::AppEventType::SdReinserted ||
               event.type == rtos::AppEventType::SdError ||
               event.type == rtos::AppEventType::StorageReadCompleted ||
               event.type == rtos::AppEventType::StorageWriteCompleted ||
               event.type == rtos::AppEventType::StorageRequestError) {
      if (event.requestId == kObsoletePendingWeightDeleteRequestId ||
          event.requestId ==
              kObsoletePendingMeasurementsDeleteRequestId ||
          event.requestId == kObsoleteSpoolCacheDeleteRequestId ||
          event.requestId == kObsoleteFilamentCacheDeleteRequestId ||
          event.requestId == kObsoleteVendorCacheDeleteRequestId) {
        if (event.type == rtos::AppEventType::StorageWriteCompleted)
          FS_LOGI(services::LogComponent::App,
                  "Obsolete storage file removed request_id=%lu",
                  static_cast<unsigned long>(event.requestId));
        else
          FS_LOGW(services::LogComponent::App,
                  "Obsolete storage file cleanup failed request_id=%lu",
                  static_cast<unsigned long>(event.requestId));
        continue;
      }
      if (auto* legacyFile = legacyFileForLoadRequest(event.requestId);
          legacyFile != nullptr) {
        legacyFile->loadFinished = true;
        legacyFile->loadFailed =
            event.type == rtos::AppEventType::StorageRequestError;
        legacyFile->exists =
            event.type == rtos::AppEventType::StorageReadCompleted &&
            event.value >= 0;
        legacyFile->count = 0;
        if (legacyFile->exists) {
          legacyFile->count = event.legacyNfcMappingCount;
          for (std::uint8_t index = 0;
               index < event.legacyNfcMappingCount; ++index)
            legacyFile->mappings[index] = event.legacyNfcMappings[index];
          FS_LOGI(services::LogComponent::App,
                  "Legacy mapping file detected path=%s entries=%u",
                  legacyFile->path,
                  static_cast<unsigned>(legacyFile->count));
        } else if (legacyFile->loadFailed) {
          FS_LOGW(services::LogComponent::App,
                  "Legacy mapping file retained path=%s reason=invalid_or_unreadable",
                  legacyFile->path);
        } else {
          FS_LOGD(services::LogComponent::App,
                  "Legacy mapping file absent path=%s", legacyFile->path);
        }
        tryStartLegacyMigration(ctx);
        continue;
      }
      if (event.requestId >= kLegacyMigrationDeleteRequestBase &&
          event.requestId <
              kLegacyMigrationDeleteRequestBase + legacyMappingFiles.size()) {
        const std::uint8_t index = static_cast<std::uint8_t>(
            event.requestId - kLegacyMigrationDeleteRequestBase);
        auto& file = legacyMappingFiles[index];
        if (event.type == rtos::AppEventType::StorageWriteCompleted) {
          FS_LOGI(services::LogComponent::App,
                  "Legacy mapping file deleted after complete migration path=%s entries=%u",
                  file.path, static_cast<unsigned>(file.count));
          file.exists = false;
        } else {
          file.migrationFailed = true;
          ++legacyMigrationConflicts;
          FS_LOGW(services::LogComponent::App,
                  "Legacy mapping file retained path=%s reason=delete_failed",
                  file.path);
        }
        legacyMigrationFileIndex = static_cast<std::uint8_t>(index + 1U);
        legacyMigrationEntryIndex = 0;
        legacyMigrationStage = LegacyMigrationStage::Waiting;
        advanceLegacyMigration(ctx);
        continue;
      }
      if (event.type == rtos::AppEventType::StorageReadCompleted &&
          event.requestId == kNetworkLoadRequestId) {
        rtos::NetworkCommand networkCommand{};
        networkCommand.type = rtos::NetworkCommandType::ApplyConfiguration;
        networkCommand.requestId = event.requestId;
        networkCommand.settings = event.networkSettings;
        sendNetworkCommand(ctx, networkCommand);
      } else if (event.type == rtos::AppEventType::StorageReadCompleted &&
                 event.requestId == kSpoolmanLoadRequestId) {
        applySpoolmanSettingsToDraft(event.spoolmanSettings);
        sendSpoolmanDraftToUi(ctx);
        rtos::SpoolmanCommand spoolman{};
        spoolman.type = rtos::SpoolmanCommandType::ApplyConfiguration;
        spoolman.requestId = event.requestId;
        spoolman.settings = event.spoolmanSettings;
        if (xQueueSend(ctx.spoolmanCommandQueue, &spoolman,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          FS_LOGW(services::LogComponent::App,
                  "Command enqueue failed queue=spoolman op=apply_configuration");
      } else if (pendingSpoolmanSaveRequestId != 0 &&
                 event.requestId == pendingSpoolmanSaveRequestId &&
                 event.type == rtos::AppEventType::StorageWriteCompleted) {
        models::SpoolmanSettings settings{};
        spoolmanSettingsFromDraft(settings);
        rtos::SpoolmanCommand spoolman{};
        spoolman.type = rtos::SpoolmanCommandType::ApplyConfiguration;
        spoolman.requestId = event.requestId;
        spoolman.settings = settings;
        xQueueSend(ctx.spoolmanCommandQueue, &spoolman, pdMS_TO_TICKS(1000));
        pendingSpoolmanSaveRequestId = 0;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, event.requestId,
                    "Spoolman gespeichert",
                    "Die normalisierte Server-URL und das Timeout wurden gespeichert.");
      } else if (pendingSpoolmanSaveRequestId != 0 &&
                 event.requestId == pendingSpoolmanSaveRequestId &&
                 event.type == rtos::AppEventType::StorageRequestError) {
        pendingSpoolmanSaveRequestId = 0;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Speichern fehlgeschlagen", event.text);
      } else if (event.type == rtos::AppEventType::StorageReadCompleted &&
                 event.requestId == kBambuLoadRequestId) {
        printerConfigs = event.bambuConfigs;
        // Bridges the persisted selection into the Phase 8.4 runtime
        // pointer, matching SetActivePrinter's behavior; falls back to the
        // default printer so Home shows something without an extra tap even
        // if the user never explicitly selected one.
        rtos::PrinterId initialFocus = printerConfigs.selectedPrinterId;
        if (!models::isValidPrinterId(initialFocus))
          initialFocus = printerConfigs.defaultPrinterId;
        if (models::isValidPrinterId(initialFocus)) {
          printerCollection.activePrinterId = initialFocus;
          printerEntry(initialFocus).isActive = true;
        }
        syncAllPrinterEntriesToUi(ctx);
        connectAllEnabledPrinters(ctx);
      } else if (event.type == rtos::AppEventType::StorageReadCompleted &&
                 event.requestId == kTraySpoolCacheLoadRequestId) {
        traySpoolCache = event.traySpoolCache;
      } else if (pendingBambuSaveRequestId != 0 &&
                 event.requestId == pendingBambuSaveRequestId &&
                 event.type == rtos::AppEventType::StorageWriteCompleted) {
        pendingBambuSaveRequestId = 0;
        if (pendingBambuSaveShowsResult) {
          if (models::isValidPrinterId(pendingBambuSaveNotifyPrinterId)) {
            syncPrinterEntryToUi(ctx, pendingBambuSaveNotifyPrinterId);
            pendingBambuSaveNotifyPrinterId = models::kInvalidPrinterId;
          }
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Success, event.requestId,
                      "Drucker gespeichert",
                      "Die Druckerkonfiguration wurde gespeichert.");
        }
      } else if (pendingBambuSaveRequestId != 0 &&
                 event.requestId == pendingBambuSaveRequestId &&
                 event.type == rtos::AppEventType::StorageRequestError) {
        pendingBambuSaveRequestId = 0;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Speichern fehlgeschlagen", event.text);
      } else if (event.type == rtos::AppEventType::StorageReadCompleted &&
          event.requestId == kScaleLoadRequestId) {
        scaleOffsetCounts = event.scaleOffsetCounts;
        scaleFactorCountsPerGram = event.scaleFactorCountsPerGram;
        scaleCalibrated = event.scaleCalibrated;
        rtos::ScaleCommand scaleCommand{};
        scaleCommand.type = rtos::ScaleCommandType::ApplyCalibration;
        scaleCommand.requestId = event.requestId;
        scaleCommand.offsetCounts = event.scaleOffsetCounts;
        scaleCommand.factorCountsPerGram = event.scaleFactorCountsPerGram;
        scaleCommand.calibrated = event.scaleCalibrated;
        sendScaleCommand(ctx, scaleCommand);
        sendScaleUiState(ctx, event.requestId);
      }
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "Storage");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status,
                    "AppTask: storage status UI queue timeout/overflow");
      if (event.type == rtos::AppEventType::SdMounted) {
        std::snprintf(bootSdStatus, sizeof(bootSdStatus), "SD-Karte: bereit");
        refreshBootProgress(ctx, event.requestId);
        storageStartupReady = true;
        requestNetworkConfiguration(ctx);
        requestSpoolmanConfiguration(ctx);
        requestBambuConfiguration(ctx);
        requestTraySpoolCache(ctx);
        requestScaleConfiguration(ctx);
        deleteObsoleteStorageFile(
            ctx, kObsoletePendingWeightDeleteRequestId,
            "/queue/pending-weight.json");
        deleteObsoleteStorageFile(
            ctx, kObsoletePendingMeasurementsDeleteRequestId,
            "/queue/pending-measurements.json");
        deleteObsoleteStorageFile(ctx, kObsoleteSpoolCacheDeleteRequestId,
                                  "/cache/spools.json");
        deleteObsoleteStorageFile(ctx, kObsoleteFilamentCacheDeleteRequestId,
                                  "/cache/filaments.json");
        deleteObsoleteStorageFile(ctx, kObsoleteVendorCacheDeleteRequestId,
                                  "/cache/vendors.json");
        rtos::StorageCommand mappingLoad{};
        mappingLoad.type = rtos::StorageCommandType::LoadJson;
        mappingLoad.requestId = kBambuMappingLoadRequestId;
        mappingLoad.documentType = rtos::StorageDocumentType::Nfc;
        std::snprintf(mappingLoad.path, sizeof(mappingLoad.path),
                      "/mappings/bambu-tags.json");
        if (xQueueSend(ctx.storageCommandQueue, &mappingLoad,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          FS_LOGW(services::LogComponent::App,
                  "Command enqueue failed queue=storage op=load_bambu_mapping");
        mappingLoad.requestId = kNfcMappingLoadRequestId;
        std::snprintf(mappingLoad.path, sizeof(mappingLoad.path),
                      "/mappings/nfc-spools.json");
        if (xQueueSend(ctx.storageCommandQueue, &mappingLoad,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          FS_LOGW(services::LogComponent::App,
                  "Command enqueue failed queue=storage op=load_nfc_mapping");
        mappingLoad.requestId = kOpenMappingLoadRequestId;
        std::snprintf(mappingLoad.path, sizeof(mappingLoad.path),
                      "/mappings/open-tags.json");
        if (xQueueSend(ctx.storageCommandQueue, &mappingLoad,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          FS_LOGW(services::LogComponent::App,
                  "Command enqueue failed queue=storage op=load_open_tag_mapping");
        showHomeWhenStartupReady(ctx);
      }
    }

    const UBaseType_t minimumStack = uxTaskGetStackHighWaterMark(nullptr);
    if (reportedMinimumStack == static_cast<UBaseType_t>(~0U) ||
        minimumStack + 256U < reportedMinimumStack) {
      reportedMinimumStack = minimumStack;
      FS_LOGD(services::LogComponent::App,
              "Stack watermark free_bytes=%u",
              static_cast<unsigned>(minimumStack));
    }
  }
}
}  // namespace filament_station::tasks
