#include "tasks/Tasks.h"

#include <SD.h>
#include <SPI.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config/BoardConfig.h"
#include "config/NfcConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/JsonStorage.h"

namespace filament_station::tasks {
namespace {

constexpr const char* kRequiredDirectories[] = {
    "/config", "/cache", "/queue", "/mappings", "/diagnostics", "/logs"};

struct InitialDocument {
  const char* path;
  rtos::StorageDocumentType type;
};

constexpr InitialDocument kInitialDocuments[] = {
    {"/config/device.json", rtos::StorageDocumentType::Device},
    {"/config/network.json", rtos::StorageDocumentType::Network},
    {"/config/spoolman.json", rtos::StorageDocumentType::Spoolman},
    {"/config/bambu.json", rtos::StorageDocumentType::Bambu},
    {"/config/ui.json", rtos::StorageDocumentType::Ui},
    {"/config/scale.json", rtos::StorageDocumentType::Scale},
    {"/config/nfc.json", rtos::StorageDocumentType::Nfc},
    {"/mappings/bambu-tags.json", rtos::StorageDocumentType::Nfc},
    {"/mappings/nfc-spools.json", rtos::StorageDocumentType::Nfc},
    {"/mappings/open-tags.json", rtos::StorageDocumentType::Nfc},
};

bool isMappingPath(const char* path) {
  return std::strcmp(path, "/mappings/bambu-tags.json") == 0 ||
         std::strcmp(path, "/mappings/nfc-spools.json") == 0 ||
         std::strcmp(path, "/mappings/open-tags.json") == 0;
}

bool parseMappingFormat(const char* text, models::TagFormat& format) {
  if (std::strcmp(text, "filamentStation") == 0)
    format = models::TagFormat::FilamentStation;
  else if (std::strcmp(text, "bambuLab") == 0) format = models::TagFormat::BambuLab;
  else if (std::strcmp(text, "openPrintTag") == 0) format = models::TagFormat::OpenPrintTag;
  else if (std::strcmp(text, "openTag3D") == 0) format = models::TagFormat::OpenTag3D;
  else if (std::strcmp(text, "legacy") == 0) format = models::TagFormat::Legacy;
  else if (std::strcmp(text, "unknown") == 0) format = models::TagFormat::Unknown;
  else return false;
  return true;
}

bool formatAllowedForPath(models::TagFormat format, const char* path) {
  if (std::strcmp(path, "/mappings/bambu-tags.json") == 0)
    return format == models::TagFormat::BambuLab;
  if (std::strcmp(path, "/mappings/open-tags.json") == 0)
    return format == models::TagFormat::OpenPrintTag ||
           format == models::TagFormat::OpenTag3D;
  return format == models::TagFormat::FilamentStation ||
         format == models::TagFormat::Unknown ||
         format == models::TagFormat::Legacy;
}

void sendStorageEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                      const char* text, std::uint32_t requestId = 0,
                      std::int32_t value = 0) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  event.value = value;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    rtos::logLine("StorageTask: appEventQueue timeout/overflow");
  }
}

bool isAllowedJsonPath(const char* path) {
  if (path == nullptr || path[0] != '/' || std::strstr(path, "..") != nullptr) {
    return false;
  }
  const std::size_t length = std::strlen(path);
  constexpr char kSuffix[] = ".json";
  if (length <= sizeof(kSuffix) - 1U ||
      std::strcmp(path + length - (sizeof(kSuffix) - 1U), kSuffix) != 0) {
    return false;
  }
  for (const char* directory : kRequiredDirectories) {
    const std::size_t directoryLength = std::strlen(directory);
    if (std::strncmp(path, directory, directoryLength) == 0 &&
        path[directoryLength] == '/' && path[directoryLength + 1U] != '\0') {
      return true;
    }
  }
  return false;
}

void sendStorageResult(rtos::RtosContext& ctx,
                       const rtos::StorageCommand& command,
                       rtos::AppEventType successType,
                       const services::JsonStorageResult& result,
                       const char* successText) {
  if (result.ok()) {
    sendStorageEvent(ctx, successType, successText, command.requestId,
                     static_cast<std::int32_t>(result.bytesProcessed));
    return;
  }
  char text[64];
  std::snprintf(text, sizeof(text), "Storage request failed: %s",
                services::JsonStorage::errorName(result.error));
  sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError, text,
                   command.requestId, static_cast<std::int32_t>(result.error));
}

void processLoadCommand(rtos::RtosContext& ctx,
                        const rtos::StorageCommand& command) {
  File file = SD.open(command.path, FILE_READ);
  if (!file) {
    sendStorageResult(ctx, command, rtos::AppEventType::StorageReadCompleted,
                      {services::JsonStorageError::FileUnavailable, 0},
                      "JSON loaded and validated");
    return;
  }
  JsonDocument document;
  const services::JsonStorageResult result =
      services::JsonStorage::load(file, command.documentType, document);
  file.close();
  if (result.ok() &&
      command.documentType == rtos::StorageDocumentType::Scale) {
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::StorageReadCompleted;
    event.requestId = command.requestId;
    event.value = static_cast<std::int32_t>(result.bytesProcessed);
    event.scaleCalibrated = document["calibrated"].as<bool>();
    event.scaleOffsetCounts = document["tareOffsetCounts"].as<std::int32_t>();
    event.scaleFactorCountsPerGram =
        document["factorCountsPerGram"].as<float>();
    std::snprintf(event.text, sizeof(event.text),
                  "Scale configuration loaded");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
      rtos::logLine("StorageTask: scale config event queue overflow");
    }
    return;
  }
  if (result.ok() && command.documentType == rtos::StorageDocumentType::Nfc &&
      isMappingPath(command.path)) {
    if (!document["mappings"].is<JsonArrayConst>()) {
      sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                       "NFC mapping array is missing", command.requestId);
      return;
    }
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::StorageReadCompleted;
    event.requestId = command.requestId;
    const JsonArrayConst mappings = document["mappings"].as<JsonArrayConst>();
    for (const JsonObjectConst mapping : mappings) {
      if (event.nfcMappingCount >= rtos::kMaximumNfcUidMappings) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "Mapping file contains too many entries",
                         command.requestId);
        return;
      }
      const char* uidText = mapping["uid"] | "";
      const char* formatText = mapping["format"] | "";
      const std::size_t uidTextLength = std::strlen(uidText);
      if (uidTextLength < 8 || uidTextLength > config::kNfcMaxUidLength * 2U ||
          (uidTextLength & 1U) != 0 ||
          !mapping["spoolId"].is<std::uint32_t>()) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "Damaged NFC mapping entry", command.requestId);
        return;
      }
      auto& destination = event.nfcMappings[event.nfcMappingCount];
      if (formatText[0] == '\0') {
        // Backward-compatible migration of phase-5.9 files.  The next save
        // writes the explicit format field required by schema version 1.
        destination.tagFormat =
            std::strcmp(command.path, "/mappings/bambu-tags.json") == 0
                ? models::TagFormat::BambuLab
                : models::TagFormat::Unknown;
      } else if (!parseMappingFormat(formatText, destination.tagFormat)) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "Invalid NFC mapping format", command.requestId);
        return;
      }
      if (
          !formatAllowedForPath(destination.tagFormat, command.path)) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "Invalid NFC mapping format", command.requestId);
        return;
      }
      destination.uidLength = static_cast<std::uint8_t>(uidTextLength / 2U);
      bool valid = true;
      for (std::size_t index = 0; index < destination.uidLength; ++index) {
        char byteText[3]{uidText[index * 2], uidText[index * 2 + 1], '\0'};
        char* end = nullptr;
        const unsigned long value = std::strtoul(byteText, &end, 16);
        if (end == nullptr || *end != '\0' || value > 0xFF) {
          valid = false;
          break;
        }
        destination.uid[index] = static_cast<std::uint8_t>(value);
      }
      destination.spoolId = mapping["spoolId"].as<std::uint32_t>();
      if (!valid || destination.spoolId == 0) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "Invalid NFC mapping UID or spool ID",
                         command.requestId);
        return;
      }
      for (std::uint8_t existing = 0; existing < event.nfcMappingCount;
           ++existing) {
        if (event.nfcMappings[existing].uidLength == destination.uidLength &&
            std::memcmp(event.nfcMappings[existing].uid, destination.uid,
                        destination.uidLength) == 0) {
          sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                           "Duplicate UID in NFC mapping file",
                           command.requestId);
          return;
        }
      }
      ++event.nfcMappingCount;
    }
    std::snprintf(event.text, sizeof(event.text), "%s UID mappings loaded",
                  std::strcmp(command.path, "/mappings/bambu-tags.json") == 0
                      ? "Bambu"
                      : std::strcmp(command.path, "/mappings/open-tags.json") == 0
                            ? "Open tag"
                            : "NFC");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
      rtos::logLine("StorageTask: mapping event queue overflow");
    return;
  }
  sendStorageResult(ctx, command, rtos::AppEventType::StorageReadCompleted,
                    result, "JSON loaded and validated");
}

void processSaveCommand(rtos::RtosContext& ctx,
                        const rtos::StorageCommand& command) {
  const std::size_t maximumSize =
      services::JsonStorage::maxSizeFor(command.documentType);
  if (command.jsonLength == 0 ||
      command.jsonLength > rtos::kStorageJsonPayloadCapacity ||
      command.jsonLength > maximumSize) {
    sendStorageResult(ctx, command, rtos::AppEventType::StorageWriteCompleted,
                      {services::JsonStorageError::InvalidArgument, 0},
                      "JSON saved atomically");
    return;
  }

  JsonDocument document;
  const DeserializationError parseError =
      deserializeJson(document, command.json, command.jsonLength);
  services::JsonStorageError error =
      parseError ? services::JsonStorageError::ParseFailed
                 : services::JsonStorage::applyDefaults(document);
  if (error == services::JsonStorageError::Ok) {
    error = services::JsonStorage::validate(document, command.documentType);
  }
  if (error != services::JsonStorageError::Ok) {
    sendStorageResult(ctx, command, rtos::AppEventType::StorageWriteCompleted,
                      {error, command.jsonLength}, "JSON saved atomically");
    return;
  }

  const services::JsonStorageResult result = services::JsonStorage::atomicSave(
      SD, command.path, command.documentType, document);
  sendStorageResult(ctx, command, rtos::AppEventType::StorageWriteCompleted,
                    result, "JSON saved atomically");
}

void processStorageCommand(rtos::RtosContext& ctx,
                           const rtos::StorageCommand& command) {
  if (std::memchr(command.path, '\0', sizeof(command.path)) == nullptr ||
      !isAllowedJsonPath(command.path)) {
    sendStorageResult(ctx, command, rtos::AppEventType::StorageRequestError,
                      {services::JsonStorageError::InvalidPath, 0}, "");
    return;
  }
  switch (command.type) {
    case rtos::StorageCommandType::LoadJson:
      processLoadCommand(ctx, command);
      return;
    case rtos::StorageCommandType::SaveJson:
      processSaveCommand(ctx, command);
      return;
    case rtos::StorageCommandType::DeleteJson:
    case rtos::StorageCommandType::CreateBackup:
      sendStorageResult(ctx, command, rtos::AppEventType::StorageRequestError,
                        {services::JsonStorageError::InvalidArgument, 0}, "");
      return;
  }
}

bool cardIsAccessible() {
  if (SD.cardType() == CARD_NONE) {
    return false;
  }

  File root = SD.open("/");
  if (!root) {
    return false;
  }
  root.close();
  return true;
}

bool ensureDirectory(const char* path) {
  File directory = SD.open(path);
  if (directory) {
    const bool isDirectory = directory.isDirectory();
    directory.close();
    return isDirectory;
  }

  if (!SD.mkdir(path)) {
    return false;
  }

  directory = SD.open(path);
  if (!directory) {
    return false;
  }
  const bool isDirectory = directory.isDirectory();
  directory.close();
  return isDirectory;
}

bool ensureDirectoryStructure() {
  for (const char* path : kRequiredDirectories) {
    if (!ensureDirectory(path)) {
      char line[96];
      std::snprintf(line, sizeof(line),
                    "StorageTask: required directory failed: %s", path);
      rtos::logLine(line);
      return false;
    }
  }
  return true;
}

bool ensureInitialDocument(const InitialDocument& definition) {
  const services::JsonStorageResult recovery =
      services::JsonStorage::recoverAtomicSave(SD, definition.path,
                                               definition.type);
  if (!recovery.ok()) {
    char line[128];
    std::snprintf(line, sizeof(line),
                  "StorageTask: recovery failed for %s: %s", definition.path,
                  services::JsonStorage::errorName(recovery.error));
    rtos::logLine(line);
    // A damaged mapping file must not make the complete SD/configuration
    // subsystem unavailable.  Keep it untouched for diagnosis; its explicit
    // load will report a StorageRequestError and no mappings will be applied.
    if (isMappingPath(definition.path)) return true;
    return false;
  }

  if (SD.exists(definition.path)) {
    File file = SD.open(definition.path, FILE_READ);
    JsonDocument document;
    const services::JsonStorageResult loaded =
        services::JsonStorage::load(file, definition.type, document);
    file.close();
    if (!loaded.ok()) {
      char line[128];
      std::snprintf(line, sizeof(line),
                    "StorageTask: invalid initial file %s: %s",
                    definition.path,
                    services::JsonStorage::errorName(loaded.error));
      rtos::logLine(line);
      return false;
    }
    return true;
  }

  JsonDocument document;
  const services::JsonStorageError defaultError =
      services::JsonStorage::createDefault(definition.type, document);
  if (defaultError != services::JsonStorageError::Ok) {
    return false;
  }
  const services::JsonStorageResult saved = services::JsonStorage::atomicSave(
      SD, definition.path, definition.type, document);
  if (!saved.ok()) {
    char line[128];
    std::snprintf(line, sizeof(line),
                  "StorageTask: initial file creation failed %s: %s",
                  definition.path,
                  services::JsonStorage::errorName(saved.error));
    rtos::logLine(line);
    return false;
  }
  char line[112];
  std::snprintf(line, sizeof(line), "StorageTask: created %s", definition.path);
  rtos::logLine(line);
  return true;
}

bool ensureInitialDocuments() {
  for (const InitialDocument& definition : kInitialDocuments) {
    if (!ensureInitialDocument(definition)) {
      return false;
    }
  }
  return true;
}

const char* cardTypeName(std::uint8_t cardType) {
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC/SDXC";
    case CARD_NONE:
      return "none";
    default:
      return "unknown";
  }
}

void logSdCardInfo() {
  const std::uint64_t cardSize = SD.cardSize();
  const std::uint64_t totalBytes = SD.totalBytes();
  const std::uint64_t usedBytes = SD.usedBytes();
  const std::uint64_t freeBytes =
      totalBytes >= usedBytes ? totalBytes - usedBytes : 0;
  constexpr std::uint64_t kBytesPerMiB = 1024ULL * 1024ULL;

  char line[112];
  std::snprintf(line, sizeof(line), "StorageTask: SD type: %s",
                cardTypeName(SD.cardType()));
  rtos::logLine(line);
  std::snprintf(line, sizeof(line),
                "StorageTask: SD capacity: %llu bytes (%llu MiB)",
                static_cast<unsigned long long>(cardSize),
                static_cast<unsigned long long>(cardSize / kBytesPerMiB));
  rtos::logLine(line);
  std::snprintf(line, sizeof(line),
                "StorageTask: filesystem total: %llu bytes (%llu MiB)",
                static_cast<unsigned long long>(totalBytes),
                static_cast<unsigned long long>(totalBytes / kBytesPerMiB));
  rtos::logLine(line);
  std::snprintf(line, sizeof(line),
                "StorageTask: filesystem used: %llu bytes (%llu MiB)",
                static_cast<unsigned long long>(usedBytes),
                static_cast<unsigned long long>(usedBytes / kBytesPerMiB));
  rtos::logLine(line);
  std::snprintf(line, sizeof(line),
                "StorageTask: filesystem free: %llu bytes (%llu MiB)",
                static_cast<unsigned long long>(freeBytes),
                static_cast<unsigned long long>(freeBytes / kBytesPerMiB));
  rtos::logLine(line);
}

}  // namespace

void storageTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  SPIClass sdSpi(FSPI);
  sdSpi.begin(config::kSdClockPin, config::kSdMisoPin, config::kSdMosiPin,
              config::kSdChipSelectPin);

  const bool mounted =
      SD.begin(config::kSdChipSelectPin, sdSpi) && cardIsAccessible();
  const bool structureReady = mounted && ensureDirectoryStructure();
  const bool initialDocumentsReady = structureReady && ensureInitialDocuments();

  if (!initialDocumentsReady) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    sendStorageEvent(ctx, rtos::AppEventType::SdError,
                     !mounted
                         ? "SD card unavailable; restart required"
                         : structureReady
                               ? "SD configuration invalid; restart required"
                               : "SD directory structure invalid; restart required");
    rtos::logLine(!mounted
                      ? "StorageTask: SD initialization failed"
                      : structureReady
                            ? "StorageTask: initial JSON setup failed"
                            : "StorageTask: SD directory setup failed");
  } else {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    logSdCardInfo();
    sendStorageEvent(ctx, rtos::AppEventType::SdMounted,
                     "SD card, directories and configuration ready");
    rtos::logLine("StorageTask: initial JSON files ready");
    char stackLine[96];
    std::snprintf(stackLine, sizeof(stackLine),
                  "StorageTask: minimum remaining stack: %u bytes",
                  static_cast<unsigned int>(
                      uxTaskGetStackHighWaterMark(nullptr)));
    rtos::logLine(stackLine);
  }

  bool removalLatched =
      (xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_SD_READY) == 0;
  bool reinsertionReported = false;
  // Der feste JSON-Puffer macht StorageCommand fuer eine Stackvariable zu
  // gross. Der Puffer bleibt statisch und gehoert weiterhin exklusiv diesem
  // einzelnen Task.
  static rtos::StorageCommand command{};
  for (;;) {
    // Kein Card-Detect vorhanden: Die Queue blockiert zwischen den bewusst
    // langsamen Zugriffsproben.
    const BaseType_t received = xQueueReceive(
        ctx.storageCommandQueue, &command,
        pdMS_TO_TICKS(config::kSdHealthCheckIntervalMs));
    if (received == pdTRUE) {
      if (removalLatched) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "SD unavailable; restart required", command.requestId,
                         static_cast<std::int32_t>(
                             services::JsonStorageError::FileUnavailable));
        rtos::logLine("StorageTask: command rejected; restart required");
      } else {
        processStorageCommand(ctx, command);
      }
    }

    const bool accessible = cardIsAccessible();
    if (!removalLatched && !accessible) {
      removalLatched = true;
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
      sendStorageEvent(ctx, rtos::AppEventType::SdRemoved,
                       "SD card removed; restart required");
      rtos::logLine("StorageTask: SD card removed; error latched");
    } else if (removalLatched && accessible && !reinsertionReported) {
      reinsertionReported = true;
      sendStorageEvent(ctx, rtos::AppEventType::SdReinserted,
                       "SD card reinserted; restart still required");
      rtos::logLine("StorageTask: SD reinserted; restart still required");
    }
  }
}
}  // namespace filament_station::tasks
