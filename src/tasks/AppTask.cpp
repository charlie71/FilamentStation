#include "tasks/Tasks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <ArduinoJson.h>
#include "config/NfcConfig.h"
#include "config/ScaleConfig.h"
#include "nfc/TagWritePolicy.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/SpoolmanUrl.h"
#include "services/SpoolmanClient.h"
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
rtos::UiOverlayKind pendingOverlay = rtos::UiOverlayKind::None;
bool wifiPortalRequested = false;
bool wifiPortalActive = false;
std::uint32_t wifiPortalRequestId = 0;
constexpr std::uint32_t kScaleLoadRequestId = 0x53430001U;
constexpr std::uint32_t kBambuMappingLoadRequestId = 0x4E464301U;
constexpr std::uint32_t kNfcMappingLoadRequestId = 0x4E464302U;
constexpr std::uint32_t kOpenMappingLoadRequestId = 0x4E464303U;
constexpr std::uint32_t kLegacyMigrationLookupRequestId = 0x4D470101U;
constexpr std::uint32_t kLegacyMigrationLoadSpoolRequestId = 0x4D470102U;
constexpr std::uint32_t kLegacyMigrationSetTagRequestId = 0x4D470103U;
constexpr std::uint32_t kLegacyMigrationDeleteRequestBase = 0x4D470110U;
constexpr std::uint32_t kNetworkLoadRequestId = 0x4E455401U;
constexpr std::uint32_t kSpoolmanLoadRequestId = 0x53504D01U;
constexpr std::uint32_t kPendingWeightLoadRequestId = 0x57475401U;
constexpr std::uint32_t kPendingWeightSaveRequestId = 0x57475402U;
constexpr std::uint32_t kPendingWeightDeleteRequestId = 0x57475403U;
constexpr std::uint32_t kPendingWeightRetryRequestId = 0x57475404U;
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
struct PendingWeightState {
  bool active = false;
  bool advanced = false;
  std::uint32_t requestId = 0;
  models::SpoolmanWeightUpdate update{};
};
PendingWeightState pendingWeight{};
bool pendingStagingSpoolSelection = false;
std::uint32_t pendingStagingSpoolRequestId = 0;
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

void loadPrinterDraft(rtos::PrinterId id) {
  printerDraft.id = id;
  if (id == 2) {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "X1C Labor");
    std::snprintf(printerDraft.host, sizeof(printerDraft.host), "192.168.1.51");
    std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "00M987654321");
    std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "87654321");
  } else if (id == 3) {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "A1 Mini Büro");
    std::snprintf(printerDraft.host, sizeof(printerDraft.host), "192.168.1.52");
    std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "030123456789");
    std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "11223344");
  } else if (id == 4) {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "Neuer Drucker");
    printerDraft.host[0] = '\0';
    printerDraft.serial[0] = '\0';
    printerDraft.accessCode[0] = '\0';
  } else {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "P1S Werkstatt");
    std::snprintf(printerDraft.host, sizeof(printerDraft.host), "192.168.1.50");
    std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "01P123456789");
    std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "12345678");
  }
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
  xTaskNotifyGive(ctx.scaleTask);
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

bool persistPendingWeight(rtos::RtosContext& ctx,
                          const models::SpoolmanWeightUpdate& update) {
  JsonDocument document;
  document["schemaVersion"] = 1;
  document["updatedAt"] = "1970-01-01T00:00:00Z";
  document["documentType"] = "pendingWeight";
  document["spoolId"] = update.spoolId;
  document["remainingWeightGrams"] = update.remainingWeightGrams;
  document["initialWeightGrams"] = update.initialWeightGrams;
  document["emptySpoolWeightGrams"] = update.emptySpoolWeightGrams;
  document["updateInitialWeight"] = update.updateInitialWeight;
  document["updateEmptySpoolWeight"] = update.updateEmptySpoolWeight;
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::SaveJson;
  command.requestId = kPendingWeightSaveRequestId;
  command.documentType = rtos::StorageDocumentType::PendingWeight;
  std::snprintf(command.path, sizeof(command.path),
                "/queue/pending-weight.json");
  const std::size_t length =
      serializeJson(document, command.json, sizeof(command.json));
  if (length == 0 || length >= sizeof(command.json)) return false;
  command.jsonLength = static_cast<std::uint16_t>(length);
  return xQueueSend(ctx.storageCommandQueue, &command,
                    pdMS_TO_TICKS(1000)) == pdPASS;
}

void deletePendingWeight(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::DeleteJson;
  command.requestId = kPendingWeightDeleteRequestId;
  command.documentType = rtos::StorageDocumentType::PendingWeight;
  std::snprintf(command.path, sizeof(command.path),
                "/queue/pending-weight.json");
  if (xQueueSend(ctx.storageCommandQueue, &command,
                 pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::App,
            "Command enqueue failed queue=storage op=delete_pending_weight");
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

void handleUiAction(rtos::RtosContext& ctx, const rtos::UiAction& action) {
  rtos::UiCommand command{};
  command.requestId = action.requestId;
  command.printerId = action.printerId;
  command.spoolId = action.spoolId;
  command.amsId = action.amsId;
  command.trayId = action.trayId;
  command.value = action.value;

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
      if ((spoolmanBits & (rtos::EVENT_SPOOLMAN_READY |
                          rtos::EVENT_SPOOLMAN_TAG_FIELD_READY)) !=
          (rtos::EVENT_SPOOLMAN_READY |
           rtos::EVENT_SPOOLMAN_TAG_FIELD_READY)) {
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
      if ((spoolmanBits & (rtos::EVENT_SPOOLMAN_READY |
                          rtos::EVENT_SPOOLMAN_TAG_FIELD_READY)) !=
          (rtos::EVENT_SPOOLMAN_READY |
           rtos::EVENT_SPOOLMAN_TAG_FIELD_READY)) {
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
      if (wifiPortalRequested || wifiPortalActive) {
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
      if (confirmedOverlay == rtos::UiOverlayKind::TagReview) {
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
        pendingWeight = {};
        pendingWeight.active = true;
        pendingWeight.requestId = action.requestId;
        pendingWeight.update.spoolId = quickWeight.spoolId;
        pendingWeight.update.remainingWeightGrams =
            quickWeight.pendingRemainingWeightGrams;
        pendingWeight.update.emptySpoolWeightGrams =
            quickWeight.emptyWeightGrams;
        if (!sendWeightUpdate(ctx, action.requestId, pendingWeight.update)) {
          persistPendingWeight(ctx, pendingWeight.update);
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Messung vorgemerkt",
                      "SpoolmanTask ist nicht erreichbar. Die Messung wurde lokal vorgemerkt.");
          return;
        }
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest, action.requestId,
                    "Gewicht speichern",
                    "Restgewicht wird an Spoolman gesendet und danach neu geladen.");
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::AdvancedWeightConfirmation) {
        if (!advancedWeight.pending) return;
        advancedWeight.pending = false;
        pendingWeight = {};
        pendingWeight.active = true;
        pendingWeight.advanced = true;
        pendingWeight.requestId = action.requestId;
        pendingWeight.update.spoolId = advancedWeight.spoolId;
        pendingWeight.update.remainingWeightGrams =
            advancedWeight.remainingWeightGrams;
        pendingWeight.update.initialWeightGrams =
            advancedWeight.initialWeightGrams;
        pendingWeight.update.emptySpoolWeightGrams =
            advancedWeight.emptyWeightGrams;
        pendingWeight.update.updateInitialWeight = true;
        pendingWeight.update.updateEmptySpoolWeight = true;
        if (!sendWeightUpdate(ctx, action.requestId, pendingWeight.update)) {
          persistPendingWeight(ctx, pendingWeight.update);
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Messung vorgemerkt",
                      "SpoolmanTask ist nicht erreichbar. Die Messung wurde lokal vorgemerkt.");
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

    case rtos::UiActionType::SelectPrinter:
      if (action.value == 1) {
        previousScreen = currentScreen;
        currentScreen = rtos::UiScreenId::PrinterSelect;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::PrinterSelect;
        sendUiCommand(ctx, command,
                      "AppTask: printer-select command queue overflow");
        return;
      }
      command.type = rtos::UiCommandType::UpdateHeader;
      if (!sendUiCommand(ctx, command,
                         "AppTask: header update command queue overflow")) {
        return;
      }
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = previousScreen;
      currentScreen = previousScreen;
      sendUiCommand(ctx, command,
                    "AppTask: printer return command queue overflow");
      return;

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
        command.screenId = rtos::UiScreenId::StagingDetails;
        currentScreen = rtos::UiScreenId::StagingDetails;
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

    case rtos::UiActionType::StartWifiPortal:
    case rtos::UiActionType::ResetWifiCredentials:
    case rtos::UiActionType::PrepareRestart:
    case rtos::UiActionType::RefreshDiagnostics:
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
      else if (action.type == rtos::UiActionType::RefreshDiagnostics) text = "Diagnose aktualisiert";
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

    case rtos::UiActionType::AddPrinter:
    case rtos::UiActionType::EditPrinter:
      loadPrinterDraft(action.type == rtos::UiActionType::AddPrinter ? 4 : action.printerId);
      currentScreen = rtos::UiScreenId::SettingsPrinterEdit;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      command.printerId = printerDraft.id;
      sendUiCommand(ctx, command, "AppTask: printer editor queue overflow");
      return;

    case rtos::UiActionType::SetActivePrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 3;
      sendUiCommand(ctx, command, "AppTask: active printer queue overflow");
      return;
    case rtos::UiActionType::TogglePrinterEnabled:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 1;
      sendUiCommand(ctx, command, "AppTask: enabled printer queue overflow");
      return;
    case rtos::UiActionType::SetDefaultPrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 2;
      sendUiCommand(ctx, command, "AppTask: default printer queue overflow");
      return;
    case rtos::UiActionType::SelectManagedPrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 0;
      sendUiCommand(ctx, command, "AppTask: printer selection queue overflow");
      return;
    case rtos::UiActionType::DeletePrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 4;
      sendUiCommand(ctx, command, "AppTask: delete printer queue overflow");
      currentScreen = rtos::UiScreenId::SettingsPrinters;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: printer return queue overflow");
      return;
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
      } else if (action.type == rtos::UiActionType::TestPrinterConnection) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::BambuConnection,
                    action.requestId, "Bambu-Verbindung",
                    "Verbindung zum gew\xC3\xA4hlten Drucker wird gepr\xC3\xBC" "ft (Mock)." );
      } else {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, action.requestId,
                    "Einstellungen g\xC3\xBCltig",
                    "Druckerkonfiguration wurde validiert (Mock)." );
      }
      return;
    }

    case rtos::UiActionType::OpenSpoolmanSettings:
      previousScreen = currentScreen;
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

    case rtos::UiActionType::ConfigureSlotFromStaging:
    case rtos::UiActionType::ConfigureSlot:
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::TraySelect;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: tray-select queue overflow");
      return;

    case rtos::UiActionType::ResetSlot:
    case rtos::UiActionType::UntagSlot:
    case rtos::UiActionType::ReapplySlot:
    case rtos::UiActionType::RefreshSlot:
      command.type = rtos::UiCommandType::ShowToast;
      std::snprintf(command.text, sizeof(command.text),
                    "Slot-Aktion vorgemerkt (%u/%u)", action.amsId,
                    action.trayId);
      sendUiCommand(ctx, command, "AppTask: slot action queue overflow");
      return;

    case rtos::UiActionType::SelectStaging:
      previousScreen = action.value == 1 ? rtos::UiScreenId::StagingDetails
                                         : currentScreen;
      currentScreen = action.value == 1 ? rtos::UiScreenId::StagingActions
                                        : rtos::UiScreenId::StagingDetails;
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
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Confirmation, action.requestId,
                    "Aktion best\xC3\xA4tigen",
                    "Diese Mock-Aktion kann eine Zuordnung entfernen. Fortfahren?");
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
          } else {
            char summary[128]{};
            std::snprintf(summary, sizeof(summary),
                          "%s\nHersteller: %s\nFilament: %s\nMaterial: %s\nFarbe: %s\nGewicht: %.0f g / Leer: %.0f g",
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

      if (event.type == rtos::AppEventType::WifiGotIp &&
          pendingWeight.active) {
        pendingWeight.requestId = kPendingWeightRetryRequestId;
        if (sendWeightUpdate(ctx, kPendingWeightRetryRequestId,
                             pendingWeight.update))
          FS_LOGI(services::LogComponent::App,
                  "Pending weight retry requested spool_id=%lu",
                  static_cast<unsigned long>(pendingWeight.update.spoolId));
      }

      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "WLAN");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status, "AppTask: WiFi status UI queue overflow");

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
      const bool advanced = pendingWeight.advanced;
      pendingWeight = {};
      quickWeight.lastMeasurementGrams = scaleWeightGrams();
      quickWeight.lastMeasurementSpoolId = event.spoolId;
      quickWeight.hasLastMeasurement = true;
      if (advanced) advancedWeight.committed = true;
      deletePendingWeight(ctx);
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: weight progress close overflow");
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
      } else if (!event.spool.extraTagValid) {
        finishLegacyMigrationEntry(ctx, false,
                                   "target_spool_tag_field_invalid");
      } else if (event.spool.extraTag[0] == '\0') {
        legacyMigrationStage = LegacyMigrationStage::SettingTarget;
        if (!sendLegacySpoolmanCommand(
                ctx, rtos::SpoolmanCommandType::SetSpoolTag,
                kLegacyMigrationSetTagRequestId, mapping.spoolId))
          finishLegacyMigrationEntry(ctx, false, "spoolman_queue_full");
      } else if (std::strcmp(event.spool.extraTag,
                             legacyMigrationIdentity.value) == 0) {
        finishLegacyMigrationEntry(ctx, true, "already_assigned");
      } else {
        finishLegacyMigrationEntry(ctx, false,
                                   "target_spool_has_other_tag");
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
    } else if (event.type == rtos::AppEventType::SpoolmanResponse) {
      if (pendingStagingSpoolRequestId != 0 &&
          event.requestId == pendingStagingSpoolRequestId) {
        if (event.value >= 0 && event.spool.id != 0) {
          pendingStagingSpoolRequestId = 0;
          rtos::UiCommand hide{};
          hide.type = rtos::UiCommandType::HideProgress;
          sendUiCommand(ctx, hide, "AppTask: staging load close overflow");
          rtos::UiCommand staging{};
          staging.type = rtos::UiCommandType::UpdateStaging;
          staging.requestId = event.requestId;
          staging.spoolId = event.spool.id;
          staging.spool = event.spool;
          sendUiCommand(ctx, staging, "AppTask: staging selection overflow");
          rtos::UiCommand toast{};
          toast.type = rtos::UiCommandType::ShowToast;
          toast.requestId = event.requestId;
          std::snprintf(toast.text, sizeof(toast.text),
                        "Spule #%lu ins Staging geladen",
                        static_cast<unsigned long>(event.spool.id));
          sendUiCommand(ctx, toast, "AppTask: staging toast overflow");
        }
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
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      status.value = 100;
      std::snprintf(status.text, sizeof(status.text), "Status: online");
      const char* version = std::strstr(event.text, "Version ");
      std::snprintf(status.title, sizeof(status.title), "%s",
                    version != nullptr ? version + 8 : "unbekannt");
      sendUiCommand(ctx, status, "AppTask: Spoolman status queue overflow");
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Success, event.requestId,
                  "Spoolman verbunden", event.text);
      tryStartLegacyMigration(ctx);
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
      if (pendingWeight.active &&
          (event.requestId == pendingWeight.requestId ||
           event.requestId == kPendingWeightRetryRequestId)) {
        const bool permanentMissingSpool =
            std::strstr(event.text, "HTTP 404") != nullptr;
        if (permanentMissingSpool) {
          pendingWeight = {};
          deletePendingWeight(ctx);
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, event.requestId,
                      "Spule nicht gefunden",
                      "Die Spule existiert nicht mehr in Spoolman. Bitte eine vorhandene Spule neu ausw\xC3\xA4hlen.");
          continue;
        }
        pendingWeight.update = event.weightUpdate.spoolId != 0
                                   ? event.weightUpdate
                                   : pendingWeight.update;
        const bool persisted = persistPendingWeight(ctx, pendingWeight.update);
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "%s\nDie Messung wurde lokal vorgemerkt und wird erneut versucht.",
            event.text[0] != '\0' ? event.text : "Unbekannter Spoolman-Fehler");
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    persisted ? "Messung vorgemerkt"
                              : "Messung nicht gespeichert",
                    persisted ? message
                              : "Spoolman-Fehler; die Pending-Messung konnte nicht auf SD gespeichert werden.");
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
      if (currentScreen == rtos::UiScreenId::TagDefinitionImport ||
          currentScreen == rtos::UiScreenId::TagLegacy) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Import fehlgeschlagen", event.text);
        continue;
      }
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.value = 100;
      std::snprintf(status.text, sizeof(status.text), "Status: offline");
      std::snprintf(status.title, sizeof(status.title), "-");
      sendUiCommand(ctx, status, "AppTask: Spoolman error status overflow");
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Error, event.requestId,
                  "Spoolman nicht erreichbar", event.text);
    } else if (event.type == rtos::AppEventType::UiCommunicationTest) {
      uiStartupReady = true;
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
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::BootProgress, event.requestId,
                  "FilamentStation startet",
                  "Display bereit. SD-Karte und Konfiguration werden gepr\xC3\xBC" "ft." );
      showHomeWhenStartupReady(ctx);
    } else if (event.type == rtos::AppEventType::SdMounted ||
               event.type == rtos::AppEventType::SdRemoved ||
               event.type == rtos::AppEventType::SdReinserted ||
               event.type == rtos::AppEventType::SdError ||
               event.type == rtos::AppEventType::StorageReadCompleted ||
               event.type == rtos::AppEventType::StorageWriteCompleted ||
               event.type == rtos::AppEventType::StorageRequestError) {
      if (event.requestId == kPendingWeightLoadRequestId) {
        if (event.type == rtos::AppEventType::StorageReadCompleted &&
            event.weightUpdate.spoolId != 0) {
          pendingWeight = {};
          pendingWeight.active = true;
          pendingWeight.requestId = kPendingWeightRetryRequestId;
          pendingWeight.update = event.weightUpdate;
          if ((xEventGroupGetBits(ctx.systemEventGroup) &
               rtos::EVENT_WIFI_CONNECTED) != 0)
            sendWeightUpdate(ctx, kPendingWeightRetryRequestId,
                             pendingWeight.update);
        }
        continue;
      }
      if (event.requestId == kPendingWeightSaveRequestId ||
          event.requestId == kPendingWeightDeleteRequestId) {
        if (event.type == rtos::AppEventType::StorageRequestError)
          FS_LOGE(services::LogComponent::App,
                  "Pending weight storage operation failed request_id=%lu",
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
        storageStartupReady = true;
        requestNetworkConfiguration(ctx);
        requestSpoolmanConfiguration(ctx);
        requestScaleConfiguration(ctx);
        rtos::StorageCommand pendingLoad{};
        pendingLoad.type = rtos::StorageCommandType::LoadJson;
        pendingLoad.requestId = kPendingWeightLoadRequestId;
        pendingLoad.documentType = rtos::StorageDocumentType::PendingWeight;
        std::snprintf(pendingLoad.path, sizeof(pendingLoad.path),
                      "/queue/pending-weight.json");
        if (xQueueSend(ctx.storageCommandQueue, &pendingLoad,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          FS_LOGW(services::LogComponent::App,
                  "Command enqueue failed queue=storage op=load_pending_weight");
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
