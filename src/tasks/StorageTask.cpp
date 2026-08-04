#include "tasks/Tasks.h"

#include <SD.h>
#include <SPI.h>
#include <cstdio>
#include <cstring>

#include "config/BoardConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/JsonStorage.h"

namespace filament_station::tasks {
namespace {

constexpr const char* kRequiredDirectories[] = {
    "/config", "/cache", "/queue", "/mappings", "/diagnostics", "/logs"};

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
    error = services::JsonStorage::validate(document);
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

  if (!structureReady) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    sendStorageEvent(ctx, rtos::AppEventType::SdError,
                     mounted
                         ? "SD directory structure invalid; restart required"
                         : "SD card unavailable; restart required");
    rtos::logLine(mounted ? "StorageTask: SD directory setup failed"
                          : "StorageTask: SD initialization failed");
  } else {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    logSdCardInfo();
    sendStorageEvent(ctx, rtos::AppEventType::SdMounted,
                     "SD card and directory structure ready");
    rtos::logLine("StorageTask: SD directory structure ready");
  }

  bool removalLatched =
      (xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_SD_READY) == 0;
  bool reinsertionReported = false;
  rtos::StorageCommand command{};
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
