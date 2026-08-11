#include "tasks/Tasks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include "config/ScaleConfig.h"
#include "nfc/TagWritePolicy.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
namespace {

rtos::UiScreenId currentScreen = rtos::UiScreenId::Boot;
rtos::UiScreenId previousScreen = rtos::UiScreenId::Home;
rtos::UiScreenId printerSettingsReturnScreen = rtos::UiScreenId::SettingsHome;
bool uiStartupReady = false;
bool storageStartupReady = false;
bool startupNavigationSent = false;
rtos::UiOverlayKind pendingOverlay = rtos::UiOverlayKind::None;
constexpr std::uint32_t kScaleLoadRequestId = 0x53430001U;
constexpr std::uint32_t kBambuMappingLoadRequestId = 0x4E464301U;
constexpr std::uint32_t kNfcMappingLoadRequestId = 0x4E464302U;
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
enum class PendingTagOperation : std::uint8_t { None, Write, Erase };
PendingTagOperation pendingTagOperation = PendingTagOperation::None;
models::TagReadResult currentTag{};
bool tagPresent = false;
rtos::SpoolId pendingTagSpoolId = 0;
rtos::SpoolId lastUsedTagSpoolId = 0;
std::array<rtos::NfcUidMapping, rtos::kMaximumNfcUidMappings> bambuMappings{};
std::uint8_t bambuMappingCount = 0;
bool pendingBambuMapping = false;
bool pendingBambuMappingSave = false;
bool pendingBambuMappingWasNew = false;
std::uint8_t pendingBambuMappingIndex = 0;
std::uint32_t pendingBambuMappingRequestId = 0;
rtos::SpoolId pendingBambuMappingSpoolId = 0;
rtos::NfcUidMapping previousBambuMapping{};
std::array<std::uint8_t, 10> pendingBambuUid{};
std::uint8_t pendingBambuUidLength = 0;
models::TagFormat pendingMappingFormat = models::TagFormat::Unknown;

rtos::SpoolId mappedNfcSpool(const models::TagReadResult& tag) {
  for (std::uint8_t index = 0; index < bambuMappingCount; ++index) {
    const auto& mapping = bambuMappings[index];
    if (mapping.uidLength == tag.uidLength &&
        std::memcmp(mapping.uid, tag.uid, tag.uidLength) == 0)
      return mapping.spoolId;
  }
  return 0;
}

void mergeNfcMappings(const rtos::AppEvent& event) {
  for (std::uint8_t source = 0; source < event.nfcMappingCount; ++source) {
    const auto& candidate = event.nfcMappings[source];
    std::uint8_t destination = 0;
    for (; destination < bambuMappingCount; ++destination) {
      if (bambuMappings[destination].uidLength == candidate.uidLength &&
          std::memcmp(bambuMappings[destination].uid, candidate.uid,
                      candidate.uidLength) == 0)
        break;
    }
    if (destination == bambuMappingCount) {
      if (bambuMappingCount >= bambuMappings.size()) break;
      ++bambuMappingCount;
    }
    bambuMappings[destination] = candidate;
  }
}

bool persistNfcMappings(rtos::RtosContext& ctx, std::uint32_t requestId) {
  rtos::StorageCommand storage{};
  storage.type = rtos::StorageCommandType::SaveJson;
  storage.requestId = requestId;
  storage.documentType = rtos::StorageDocumentType::Nfc;
  std::snprintf(storage.path, sizeof(storage.path),
                "/mappings/nfc-spools.json");
  std::size_t used = static_cast<std::size_t>(std::snprintf(
      storage.json, sizeof(storage.json),
      "{\"schemaVersion\":1,\"updatedAt\":\"1970-01-01T00:00:00Z\","
      "\"documentType\":\"nfc\",\"tagSchemaVersion\":1,\"mappings\":["));
  for (std::uint8_t index = 0; index < bambuMappingCount; ++index) {
    const auto& mapping = bambuMappings[index];
    int written = std::snprintf(storage.json + used,
                                sizeof(storage.json) - used,
                                "%s{\"uid\":\"", index == 0 ? "" : ",");
    if (written <= 0 || static_cast<std::size_t>(written) >=
                            sizeof(storage.json) - used)
      return false;
    used += static_cast<std::size_t>(written);
    for (std::uint8_t uidIndex = 0; uidIndex < mapping.uidLength; ++uidIndex) {
      written = std::snprintf(storage.json + used,
                              sizeof(storage.json) - used, "%02X",
                              mapping.uid[uidIndex]);
      if (written != 2 || static_cast<std::size_t>(written) >=
                              sizeof(storage.json) - used)
        return false;
      used += 2;
    }
    written = std::snprintf(storage.json + used,
                            sizeof(storage.json) - used,
                            "\",\"spoolId\":%lu}",
                            static_cast<unsigned long>(mapping.spoolId));
    if (written <= 0 || static_cast<std::size_t>(written) >=
                            sizeof(storage.json) - used)
      return false;
    used += static_cast<std::size_t>(written);
  }
  if (used + 3 > sizeof(storage.json)) return false;
  storage.json[used++] = ']';
  storage.json[used++] = '}';
  storage.json[used] = '\0';
  storage.jsonLength = static_cast<std::uint16_t>(used);
  return xQueueSend(ctx.storageCommandQueue, &storage,
                    pdMS_TO_TICKS(1000)) == pdPASS;
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
                   const char* failureMessage) {
  if (xQueueSend(ctx.uiCommandQueue, &command, pdMS_TO_TICKS(1000)) == pdPASS) {
    return true;
  }
  rtos::logLine(failureMessage);
  return false;
}

bool sendScaleCommand(rtos::RtosContext& ctx,
                      const rtos::ScaleCommand& command) {
  if (xQueueSend(ctx.scaleCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    rtos::logLine("AppTask: scaleCommandQueue timeout/overflow");
    return false;
  }
  xTaskNotifyGive(ctx.scaleTask);
  return true;
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
    rtos::logLine("AppTask: scale config load queue timeout/overflow");
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
    rtos::logLine("AppTask: scale configuration serialization failed");
    return false;
  }
  command.jsonLength = static_cast<std::uint16_t>(length);
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    rtos::logLine("AppTask: scale config save queue timeout/overflow");
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
    case rtos::UiActionType::Cancel:
      pendingBambuMapping = false;
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
        command.screenId = rtos::UiScreenId::TagActionSelect;
        currentScreen = command.screenId;
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
        pendingBambuMapping = true;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::SpoolPicker, action.requestId,
                    "Spoolman-Spule ausw\xC3\xA4hlen", "");
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
                        ? "Tag wird geschrieben"
                        : "Tag wird gel\xC3\xB6scht",
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
        quickWeight.lastMeasurementGrams =
            quickWeight.pendingGrossWeightGrams;
        quickWeight.lastMeasurementSpoolId = quickWeight.spoolId;
        quickWeight.hasLastMeasurement = true;
        quickWeight.pending = false;
        char result[96];
        std::snprintf(result, sizeof(result),
                      "Spule #%lu: %.1f g brutto, %.1f g Restgewicht.",
                      static_cast<unsigned long>(quickWeight.spoolId),
                      static_cast<double>(quickWeight.lastMeasurementGrams),
                      static_cast<double>(quickWeight.pendingRemainingWeightGrams));
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, action.requestId,
                    "Messung best\xC3\xA4tigt", result);
        return;
      }
      if (confirmedOverlay == rtos::UiOverlayKind::AdvancedWeightConfirmation) {
        if (!advancedWeight.pending) return;
        advancedWeight.pending = false;
        advancedWeight.committed = true;
        char result[128];
        std::snprintf(result, sizeof(result),
                      "Spule: #%lu\nRestgewicht: %.1f g\nLeergewicht: %.1f g\nAusgangsgewicht: %.1f g",
                      static_cast<unsigned long>(advancedWeight.spoolId),
                      static_cast<double>(advancedWeight.remainingWeightGrams),
                      static_cast<double>(advancedWeight.emptyWeightGrams),
                      static_cast<double>(advancedWeight.initialWeightGrams));
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::AdvancedWeightResult, action.requestId,
                    "Erweitertes Wiegen best\xC3\xA4tigt", result);
        return;
      }
      const char* result = "Mock-Aktion best\xC3\xA4tigt; keine reale Funktion ausgef\xC3\xBChrt.";
      if (confirmedOverlay == rtos::UiOverlayKind::RestartConfirmation) {
        result = "Neustart best\xC3\xA4tigt; im Mock nicht ausgef\xC3\xBChrt.";
      } else if (confirmedOverlay == rtos::UiOverlayKind::WifiResetConfirmation) {
        result = "WLAN-Zur\xC3\xBC" "cksetzen best\xC3\xA4tigt; Zugangsdaten bleiben im Mock erhalten.";
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
        command.screenId = rtos::UiScreenId::TagActionSelect;
        currentScreen = command.screenId;
        pendingTagOperation = PendingTagOperation::None;
      } else if (currentScreen == rtos::UiScreenId::TagActionSelect ||
                 currentScreen == rtos::UiScreenId::TagResult) {
        command.screenId = rtos::UiScreenId::StagingActions;
        currentScreen = command.screenId;
      } else {
        command.screenId = previousScreen;
        currentScreen = previousScreen;
      }
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
      return;
    }

    case rtos::UiActionType::StartWifiPortal:
    case rtos::UiActionType::ResetWifiCredentials:
    case rtos::UiActionType::PrepareRestart:
    case rtos::UiActionType::RefreshDiagnostics:
    case rtos::UiActionType::CheckFirmwareUpdate: {
      if (action.type == rtos::UiActionType::StartWifiPortal) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::ConnectionProgress,
                    action.requestId, "Verbindung wird vorbereitet",
                    "WLAN-Konfiguration wird gestartet (Mock)." );
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
      } else if (action.type == rtos::UiActionType::TestSpoolmanConnection) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest,
                    action.requestId, "Spoolman-Anfrage",
                    "Serverstatus wird abgefragt (Mock)." );
      } else {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, action.requestId,
                    "Einstellungen g\xC3\xBCltig",
                    "Spoolman-Konfiguration wurde validiert (Mock)." );
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
      sendUiCommand(ctx, command,
                    "AppTask: staging screen command queue overflow");
      return;

    case rtos::UiActionType::ImportTagDefinition: {
      if ((currentTag.format != models::TagFormat::OpenPrintTag &&
           currentTag.format != models::TagFormat::OpenTag3D) ||
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
    case rtos::UiActionType::WriteTag:
    case rtos::UiActionType::LinkTag:
    case rtos::UiActionType::UnlinkTag:
    case rtos::UiActionType::EraseTag:
    case rtos::UiActionType::SearchSpool:
    case rtos::UiActionType::SelectSpool:
      if (action.type == rtos::UiActionType::SearchSpool &&
          currentScreen == rtos::UiScreenId::TagDefinitionImport) {
        pendingBambuMapping = true;
        pendingMappingFormat = currentTag.format;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::SpoolPicker, action.requestId,
                    "Spoolman-Spule ausw\xC3\xA4hlen", "");
        return;
      }
      if (pendingBambuMapping) {
        if (action.spoolId == 0 || pendingBambuUidLength == 0 ||
            pendingBambuUidLength > pendingBambuUid.size()) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Zuordnung fehlgeschlagen", "Keine g\xC3\xBCltige Spule ausgew\xC3\xA4hlt.");
          return;
        }
        std::uint8_t index = 0;
        for (; index < bambuMappingCount; ++index) {
          if (bambuMappings[index].uidLength == pendingBambuUidLength &&
              std::memcmp(bambuMappings[index].uid, pendingBambuUid.data(),
                          pendingBambuUidLength) == 0)
            break;
        }
        if (index == bambuMappingCount) {
          if (bambuMappingCount >= bambuMappings.size()) {
            sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                        rtos::UiOverlayKind::Error, action.requestId,
                        "Zuordnung fehlgeschlagen", "Der lokale Mapping-Speicher ist voll.");
            return;
          }
          ++bambuMappingCount;
        }
        auto& mapping = bambuMappings[index];
        previousBambuMapping = mapping;
        pendingBambuMappingWasNew = index + 1U == bambuMappingCount &&
                                    mapping.uidLength == 0;
        pendingBambuMappingIndex = index;
        mapping.uidLength = pendingBambuUidLength;
        std::memcpy(mapping.uid, pendingBambuUid.data(), pendingBambuUidLength);
        mapping.spoolId = action.spoolId;
        pendingBambuMapping = false;
        command.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, command, "AppTask: Bambu picker close overflow");
        if (!persistNfcMappings(ctx, action.requestId)) {
          mapping = previousBambuMapping;
          if (pendingBambuMappingWasNew) --bambuMappingCount;
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Speichern fehlgeschlagen",
                      "Die UID-Zuordnung konnte nicht an StorageTask gesendet werden.");
          return;
        }
        pendingBambuMappingSave = true;
        pendingBambuMappingRequestId = action.requestId;
        pendingBambuMappingSpoolId = action.spoolId;
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::BambuMappingSave, action.requestId,
                    "Zuordnung speichern",
                    "Die lokale UID-Zuordnung wird gespeichert.");
        return;
      }
      if (action.type == rtos::UiActionType::SearchSpool &&
          currentScreen == rtos::UiScreenId::TagActionSelect) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::SpoolPicker, action.requestId,
                    "Spoolman-Spule ausw\xC3\xA4hlen", "");
        return;
      }
      if (currentScreen == rtos::UiScreenId::TagActionSelect) {
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
        currentScreen = command.screenId;
        sendUiCommand(ctx, command, "AppTask: NFC review screen queue overflow");
        return;
      }
      if (action.type == rtos::UiActionType::LinkTag) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::NfcRead, action.requestId,
                    "NFC-Tag lesen", "Tag bitte am Leser belassen (Mock)." );
        return;
      }
      if (action.type == rtos::UiActionType::WriteTag) {
        if (!tagPresent) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Kein NFC-Tag", "Bitte einen nativen NTAG auflegen.");
          return;
        }
        if (!currentTag.writable) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Tag ist schreibgesch\xC3\xBCtzt",
                      "Nur leere oder native beschreibbare NTAGs sind zul\xC3\xA4ssig.");
          return;
        }
        pendingTagSpoolId = action.spoolId != 0 ? action.spoolId
                                                 : lastUsedTagSpoolId;
        if (pendingTagSpoolId == 0) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Keine Spule ausgew\xC3\xA4hlt",
                      "Vor dem Schreiben muss eine Spoolman-Spule gew\xC3\xA4hlt werden.");
          return;
        }
        pendingTagOperation = PendingTagOperation::Write;
        char review[120]{};
        std::snprintf(review, sizeof(review),
                      "Spoolman-ID: %lu\nPayload: spoolman:%lu\nDer Tag wird danach erneut gelesen.",
                      static_cast<unsigned long>(pendingTagSpoolId),
                      static_cast<unsigned long>(pendingTagSpoolId));
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::TagReview;
        std::snprintf(command.text, sizeof(command.text), "%s", review);
        currentScreen = command.screenId;
        sendUiCommand(ctx, command, "AppTask: NFC review screen queue overflow");
        return;
      }
      if (action.type == rtos::UiActionType::EraseTag) {
        if (!tagPresent || !currentTag.erasable) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Tag kann nicht gel\xC3\xB6scht werden",
                      "Nur unterst\xC3\xBCtzte native NTAGs d\xC3\xBCrfen gel\xC3\xB6scht werden.");
          return;
        }
        pendingTagOperation = PendingTagOperation::Erase;
        pendingTagSpoolId = 0;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::TagReview;
        std::snprintf(command.text, sizeof(command.text),
                      "Tag: nativer NTAG21x\nAktion: NDEF-Zuordnung l\xC3\xB6schen\nDanach wird der leere Zustand verifiziert.");
        currentScreen = command.screenId;
        sendUiCommand(ctx, command, "AppTask: NFC erase review queue overflow");
        return;
      }
      if (action.type == rtos::UiActionType::ClearStaging ||
          action.type == rtos::UiActionType::UnlinkTag) {
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
      rtos::logLine("AppTask: unhandled UiAction");
      return;
  }
}

}  // namespace

void appTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::AppEvent event{};
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
        if (nativeTechnology && nativeFormat) {
          const char* chip =
              currentTag.technology == models::TagTechnology::Ntag213
                  ? "NTAG213"
                  : (currentTag.technology == models::TagTechnology::Ntag215
                         ? "NTAG215"
                         : "NTAG216");
          if (currentTag.definition.hasSpoolId)
            pendingTagSpoolId = currentTag.definition.spoolId;
          rtos::UiCommand navigation{};
          navigation.type = rtos::UiCommandType::ShowScreen;
          navigation.screenId = rtos::UiScreenId::TagActionSelect;
          navigation.spoolId = pendingTagSpoolId;
          std::snprintf(navigation.text, sizeof(navigation.text),
                        "%s | %s | %s", chip,
                        currentTag.format == models::TagFormat::FilamentStation
                            ? "FilamentStation"
                            : "leer",
                        currentTag.writable ? "beschreibbar"
                                            : "schreibgesch\xC3\xBCtzt");
          if (sendUiCommand(ctx, navigation,
                            "AppTask: NFC action screen queue overflow")) {
            previousScreen = currentScreen;
            currentScreen = navigation.screenId;
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
            pendingBambuUidLength = currentTag.uidLength;
            std::memcpy(pendingBambuUid.data(), currentTag.uid,
                        currentTag.uidLength);
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
            pendingBambuUidLength = currentTag.uidLength;
            std::memcpy(pendingBambuUid.data(), currentTag.uid,
                        currentTag.uidLength);
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
        }
      } else if (event.type == rtos::AppEventType::NfcTagRemoved) {
        tagPresent = false;
        currentTag = {};
      } else if (event.type == rtos::AppEventType::NfcTagWritten) {
        lastUsedTagSpoolId = event.spoolId;
        pendingTagOperation = PendingTagOperation::None;
        rtos::UiCommand result{};
        result.type = rtos::UiCommandType::ShowScreen;
        result.screenId = rtos::UiScreenId::TagResult;
        result.requestId = event.requestId;
        result.spoolId = event.spoolId;
        std::snprintf(result.text, sizeof(result.text),
                      "Tag erfolgreich mit Spule %lu verbunden. UID und Payload wurden verifiziert.",
                      static_cast<unsigned long>(event.spoolId));
        currentScreen = result.screenId;
        sendUiCommand(ctx, result, "AppTask: NFC result screen queue overflow");
      } else if (event.type == rtos::AppEventType::NfcTagErased) {
        pendingTagOperation = PendingTagOperation::None;
        rtos::UiCommand result{};
        result.type = rtos::UiCommandType::ShowScreen;
        result.screenId = rtos::UiScreenId::TagResult;
        result.requestId = event.requestId;
        std::snprintf(result.text, sizeof(result.text),
                      "NFC-Tag gel\xC3\xB6scht. Der leere NDEF-Zustand wurde verifiziert.");
        currentScreen = result.screenId;
        sendUiCommand(ctx, result, "AppTask: NFC erase result queue overflow");
      } else if (event.type == rtos::AppEventType::NfcError &&
                 pendingTagOperation != PendingTagOperation::None) {
        pendingTagOperation = PendingTagOperation::None;
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
    } else if (event.type == rtos::AppEventType::SpoolmanError) {
      rtos::UiCommand hide{};
      hide.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, hide, "AppTask: Spoolman progress close overflow");
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Error, event.requestId,
                  "Spoolman-Import nicht verf\xC3\xBCgbar", event.text);
    } else if (event.type == rtos::AppEventType::UiCommunicationTest) {
      uiStartupReady = true;
      rtos::UiCommand response{};
      response.type = rtos::UiCommandType::CommunicationTestResponse;
      response.requestId = event.requestId;
      std::snprintf(response.title, sizeof(response.title), "RTOS test");
      std::snprintf(response.text, sizeof(response.text), "AppTask acknowledged event");
      if (sendUiCommand(ctx, response,
                        "AppTask: uiCommandQueue timeout/overflow")) {
        rtos::logLine("AppTask: response sent");
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
      if (event.type == rtos::AppEventType::StorageReadCompleted &&
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
      } else if (event.type == rtos::AppEventType::StorageReadCompleted &&
                 (event.requestId == kBambuMappingLoadRequestId ||
                  event.requestId == kNfcMappingLoadRequestId)) {
        mergeNfcMappings(event);
      } else if (pendingBambuMappingSave &&
                 event.requestId == pendingBambuMappingRequestId &&
                 event.type == rtos::AppEventType::StorageWriteCompleted) {
        pendingBambuMappingSave = false;
        rtos::UiCommand hide{};
        hide.type = rtos::UiCommandType::HideProgress;
        sendUiCommand(ctx, hide, "AppTask: Bambu save progress close overflow");
        rtos::UiCommand result{};
        result.type = rtos::UiCommandType::ShowScreen;
        result.screenId = rtos::UiScreenId::TagResult;
        result.spoolId = pendingBambuMappingSpoolId;
        std::snprintf(result.text, sizeof(result.text),
                      "%s read-only mit Spule %lu verkn\xC3\xBCpft. Der Tag wurde nicht ver\xC3\xA4ndert.",
                      pendingMappingFormat == models::TagFormat::OpenPrintTag
                          ? "OpenPrintTag"
                          : pendingMappingFormat == models::TagFormat::OpenTag3D
                                ? "OpenTag3D"
                                : "Bambu-Tag",
                      static_cast<unsigned long>(pendingBambuMappingSpoolId));
        currentScreen = result.screenId;
        sendUiCommand(ctx, result, "AppTask: Bambu mapping result overflow");
      } else if (pendingBambuMappingSave &&
                 event.requestId == pendingBambuMappingRequestId &&
                 event.type == rtos::AppEventType::StorageRequestError) {
        pendingBambuMappingSave = false;
        bambuMappings[pendingBambuMappingIndex] = previousBambuMapping;
        if (pendingBambuMappingWasNew) --bambuMappingCount;
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Speichern fehlgeschlagen",
                    "Die UID-Zuordnung wurde nicht dauerhaft gespeichert.");
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
        requestScaleConfiguration(ctx);
        rtos::StorageCommand mappingLoad{};
        mappingLoad.type = rtos::StorageCommandType::LoadJson;
        mappingLoad.requestId = kBambuMappingLoadRequestId;
        mappingLoad.documentType = rtos::StorageDocumentType::Nfc;
        std::snprintf(mappingLoad.path, sizeof(mappingLoad.path),
                      "/mappings/bambu-tags.json");
        if (xQueueSend(ctx.storageCommandQueue, &mappingLoad,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          rtos::logLine("AppTask: Bambu mapping load queue overflow");
        mappingLoad.requestId = kNfcMappingLoadRequestId;
        std::snprintf(mappingLoad.path, sizeof(mappingLoad.path),
                      "/mappings/nfc-spools.json");
        if (xQueueSend(ctx.storageCommandQueue, &mappingLoad,
                       pdMS_TO_TICKS(1000)) != pdPASS)
          rtos::logLine("AppTask: NFC mapping load queue overflow");
        showHomeWhenStartupReady(ctx);
      }
    }
  }
}
}  // namespace filament_station::tasks
