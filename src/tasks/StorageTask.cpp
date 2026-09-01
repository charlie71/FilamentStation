/**
 * @file
 * @brief Implements tasks::storageTask(): SD-card mount/health-check,
 *        directory/initial-document bootstrap, and JSON load/save/delete
 *        command processing.
 */
#include "tasks/Tasks.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <mbedtls/sha256.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config/BoardConfig.h"
#include "config/BambuMaterialConfig.h"
#include "config/NfcConfig.h"
#include "models/BambuMaterialMapping.h"
#include "models/BambuPrinterConfig.h"
#include "models/TraySpoolCache.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/BambuMaterialCatalog.h"
#include "services/JsonStorage.h"
#include "services/Logger.h"
#include "services/PsramAlloc.h"
#include "services/Sha256Hex.h"

namespace filament_station::tasks {
namespace {

constexpr const char* kRequiredDirectories[] = {
    "/config", "/cache", "/queue", "/mappings", "/diagnostics", "/logs"};  ///< Top-level directories created on the SD card at boot.

/// @brief One config document created/recovered at boot if missing.
struct InitialDocument {
  const char* path;                ///< Absolute "/....json" path.
  rtos::StorageDocumentType type;  ///< Document type, for defaulting/validation.
};

/// @brief Every config document ensureInitialDocuments() creates/recovers at boot.
constexpr InitialDocument kInitialDocuments[] = {
    {"/config/device.json", rtos::StorageDocumentType::Device},
    {"/config/network.json", rtos::StorageDocumentType::Network},
    {"/config/spoolman.json", rtos::StorageDocumentType::Spoolman},
    {"/config/bambu.json", rtos::StorageDocumentType::Bambu},
    {"/config/ui.json", rtos::StorageDocumentType::Ui},
    {"/config/scale.json", rtos::StorageDocumentType::Scale},
    {"/config/nfc.json", rtos::StorageDocumentType::Nfc},
    {"/mappings/printer-slots.json", rtos::StorageDocumentType::TraySpoolCache},
};

/// @brief Whether a path is one of the legacy NFC UID-mapping files.
/// @param path Path to check.
/// @return true for "/mappings/bambu-tags.json", "/mappings/nfc-spools.json", or "/mappings/open-tags.json".
bool isMappingPath(const char* path) {
  return std::strcmp(path, "/mappings/bambu-tags.json") == 0 ||
         std::strcmp(path, "/mappings/nfc-spools.json") == 0 ||
         std::strcmp(path, "/mappings/open-tags.json") == 0;
}

/// @brief Parses a legacy mapping file's "format" text field.
/// @param text Format string as stored in the mapping file.
/// @param format Out parameter receiving the parsed format.
/// @return false if `text` is not one of the recognized format names.
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

/// @brief Whether a tag format is permitted in a given legacy mapping file.
/// @param format Format to check.
/// @param path Mapping file path.
/// @return true if `format` belongs in `path`.
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

/// @brief Sends a simple numeric/text AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param text Text payload.
/// @param requestId Correlation id.
/// @param value Numeric payload.
void sendStorageEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                      const char* text, std::uint32_t requestId = 0,
                      std::int32_t value = 0) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  event.value = value;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Storage,
            "Event enqueue failed queue=app_event event=%u",
            static_cast<unsigned>(type));
  }
}

/// @brief Validates a JSON path: absolute, no "..", ".json" suffix, inside a known top-level directory.
/// @param path Path to validate.
/// @return true if allowed.
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

/// @brief Sends the appropriate success/error AppEvent for a JsonStorage result.
/// @param ctx Owning RTOS context.
/// @param command Originating command, for its requestId.
/// @param successType Event type to send on success.
/// @param result Storage operation result.
/// @param successText Text to send on success.
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

/// @brief Handles a LoadJson command: opens, parses, validates, and reports
///        the file, decoding it into the type-specific rtos::AppEvent
///        fields for the document types AppTask needs structured (Network,
///        Scale, Spoolman, Bambu, TraySpoolCache, legacy NFC mappings).
/// @param ctx Owning RTOS context.
/// @param command Command to process.
void processLoadCommand(rtos::RtosContext& ctx,
                        const rtos::StorageCommand& command) {
  File file = SD.open(command.path, FILE_READ);
  if (!file) {
    if (command.documentType == rtos::StorageDocumentType::Nfc &&
        isMappingPath(command.path)) {
      rtos::AppEvent event{};
      event.type = rtos::AppEventType::StorageReadCompleted;
      event.requestId = command.requestId;
      event.value = -1;
      std::snprintf(event.text, sizeof(event.text),
                    "Legacy mapping file not found");
      if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
        FS_LOGW(services::LogComponent::Storage,
                "Event enqueue failed queue=app_event document=legacy_mapping_missing");
      return;
    }
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
      command.documentType == rtos::StorageDocumentType::Network) {
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::StorageReadCompleted;
    event.requestId = command.requestId;
    event.value = static_cast<std::int32_t>(result.bytesProcessed);
    auto& settings = event.networkSettings;
    std::snprintf(settings.hostname, sizeof(settings.hostname), "%s",
                  document["hostname"].as<const char*>());
    settings.dhcp = document["dhcp"].as<bool>();
    std::snprintf(settings.ipAddress, sizeof(settings.ipAddress), "%s",
                  document["ipAddress"].as<const char*>());
    std::snprintf(settings.gateway, sizeof(settings.gateway), "%s",
                  document["gateway"].as<const char*>());
    std::snprintf(settings.subnetMask, sizeof(settings.subnetMask), "%s",
                  document["subnetMask"].as<const char*>());
    std::snprintf(settings.dns, sizeof(settings.dns), "%s",
                  document["dns"].as<const char*>());
    std::snprintf(settings.portalName, sizeof(settings.portalName), "%s",
                  document["portalName"].as<const char*>());
    settings.portalTimeoutSeconds =
        document["portalTimeoutSeconds"].as<std::uint16_t>();
    settings.connectTimeoutSeconds =
        document["connectTimeoutSeconds"].as<std::uint16_t>();
    std::snprintf(event.text, sizeof(event.text),
                  "Network configuration loaded");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
      FS_LOGW(services::LogComponent::Storage,
              "Event enqueue failed queue=app_event document=network");
    }
    return;
  }
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
      FS_LOGW(services::LogComponent::Storage,
              "Event enqueue failed queue=app_event document=scale");
    }
    return;
  }
  if (result.ok() &&
      command.documentType == rtos::StorageDocumentType::Spoolman) {
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::StorageReadCompleted;
    event.requestId = command.requestId;
    event.value = static_cast<std::int32_t>(result.bytesProcessed);
    event.spoolmanSettings.enabled = document["enabled"].as<bool>();
    std::snprintf(event.spoolmanSettings.name,
                  sizeof(event.spoolmanSettings.name), "%s",
                  document["name"].as<const char*>());
    std::snprintf(event.spoolmanSettings.serverUrl,
                  sizeof(event.spoolmanSettings.serverUrl), "%s",
                  document["serverUrl"].as<const char*>());
    event.spoolmanSettings.timeoutMs = document["timeoutMs"].as<std::uint32_t>();
    std::snprintf(event.text, sizeof(event.text),
                  "Spoolman configuration loaded");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
      FS_LOGW(services::LogComponent::Storage,
              "Event enqueue failed queue=app_event document=spoolman");
    return;
  }
  if (result.ok() && command.documentType == rtos::StorageDocumentType::Bambu) {
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::StorageReadCompleted;
    event.requestId = command.requestId;
    event.value = static_cast<std::int32_t>(result.bytesProcessed);
    auto& configs = event.bambuConfigs;
    configs.selectedPrinterId =
        document["selectedPrinterId"].as<std::uint16_t>();
    configs.defaultPrinterId = document["defaultPrinterId"].as<std::uint16_t>();
    const JsonArrayConst printers = document["printers"].as<JsonArrayConst>();
    std::uint8_t count = 0;
    for (const JsonObjectConst printer : printers) {
      if (count >= models::kMaximumPrinters) break;
      models::BambuPrinterConfig& destination = configs.printers[count];
      destination.printerId = printer["printerId"].as<std::uint16_t>();
      std::snprintf(destination.name, sizeof(destination.name), "%s",
                    printer["name"].as<const char*>());
      std::snprintf(destination.host, sizeof(destination.host), "%s",
                    printer["host"].as<const char*>());
      std::snprintf(destination.serialNumber, sizeof(destination.serialNumber),
                    "%s", printer["serialNumber"].as<const char*>());
      std::snprintf(destination.accessCode, sizeof(destination.accessCode),
                    "%s", printer["accessCode"].as<const char*>());
      destination.enabled = printer["enabled"].as<bool>();
      destination.isDefault = printer["default"].as<bool>();
      destination.isSelected = printer["selected"].as<bool>();
      ++count;
    }
    configs.printerCount = count;
    std::snprintf(event.text, sizeof(event.text), "Bambu configuration loaded");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
      FS_LOGW(services::LogComponent::Storage,
              "Event enqueue failed queue=app_event document=bambu");
    return;
  }
  if (result.ok() &&
      command.documentType == rtos::StorageDocumentType::TraySpoolCache) {
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::StorageReadCompleted;
    event.requestId = command.requestId;
    event.value = static_cast<std::int32_t>(result.bytesProcessed);
    auto& cache = event.traySpoolCache;
    const JsonArrayConst entries = document["entries"].as<JsonArrayConst>();
    std::uint8_t count = 0;
    for (const JsonObjectConst entry : entries) {
      if (count >= models::kMaximumTraySpoolCacheEntries) break;
      models::TraySpoolCacheEntry& destination = cache.entries[count];
      destination.printerId = entry["printerId"].as<std::uint16_t>();
      destination.amsId = entry["amsId"].as<std::uint8_t>();
      destination.trayId = entry["trayId"].as<std::uint8_t>();
      destination.spoolId = entry["spoolId"].as<std::uint32_t>();
      std::snprintf(destination.material, sizeof(destination.material), "%s",
                    entry["material"].as<const char*>());
      std::snprintf(destination.colorHex, sizeof(destination.colorHex), "%s",
                    entry["colorHex"].as<const char*>());
      ++count;
    }
    cache.entryCount = count;
    std::snprintf(event.text, sizeof(event.text),
                  "Tray-Spoolman cache loaded");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
      FS_LOGW(services::LogComponent::Storage,
              "Event enqueue failed queue=app_event document=tray_spool_cache");
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
      if (event.legacyNfcMappingCount >= rtos::kMaximumLegacyNfcMappings) {
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
      auto& destination =
          event.legacyNfcMappings[event.legacyNfcMappingCount];
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
      for (std::uint8_t existing = 0;
           existing < event.legacyNfcMappingCount;
           ++existing) {
        if (event.legacyNfcMappings[existing].uidLength == destination.uidLength &&
            std::memcmp(event.legacyNfcMappings[existing].uid, destination.uid,
                        destination.uidLength) == 0) {
          sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                           "Duplicate UID in NFC mapping file",
                           command.requestId);
          return;
        }
      }
      ++event.legacyNfcMappingCount;
    }
    std::snprintf(event.text, sizeof(event.text), "%s UID mappings loaded",
                  std::strcmp(command.path, "/mappings/bambu-tags.json") == 0
                      ? "Bambu"
                      : std::strcmp(command.path, "/mappings/open-tags.json") == 0
                            ? "Open tag"
                            : "NFC");
    if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
      FS_LOGW(services::LogComponent::Storage,
              "Event enqueue failed queue=app_event document=tag_mapping");
    return;
  }
  sendStorageResult(ctx, command, rtos::AppEventType::StorageReadCompleted,
                    result, "JSON loaded and validated");
}

/// @brief Handles a SaveJson command: parses the inline payload, validates
///        it, and atomically saves it to `command.path`.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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

// ---------------------------------------------------------------------
// Bambu material-mapping table: boot-time load, and download activation
// (TASKS.md Nachtrag 2026-08-28). See docs/bambu-protocol.md.
// ---------------------------------------------------------------------

/// @brief Sends a BambuMaterialUpdateResult AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param success Whether the update succeeded.
/// @param text Result text (error message on failure).
void sendBambuMaterialUpdateResult(rtos::RtosContext& ctx,
                                   std::uint32_t requestId, bool success,
                                   const char* text) {
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::BambuMaterialUpdateResult;
  event.requestId = requestId;
  event.value = success ? 1 : 0;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Storage,
            "Event enqueue failed queue=app_event "
            "event=bambu_material_update_result");
  }
}

/// @brief Number of '|'-separated match values across every rule's three
///        match categories in `table` (for the "match_values=" field of the
///        "[BAMBU] Material mappings loaded" log line) -- an analogous
///        richness metric to the old schema-v1 alias count.
std::uint32_t countBambuMaterialMatchValues(
    const models::BambuMaterialRuleTable& table) {
  std::uint32_t count = 0;
  const char* categories[3] = {nullptr, nullptr, nullptr};
  for (std::uint16_t index = 0; index < table.ruleCount; ++index) {
    const models::BambuMaterialMatch& match = table.rules[index].match;
    categories[0] = match.materialExact;
    categories[1] = match.nameContainsAny;
    categories[2] = match.manufacturerExact;
    for (const char* cursor : categories) {
      if (*cursor == '\0') continue;
      ++count;
      for (; *cursor != '\0'; ++cursor) {
        if (*cursor == models::kBambuMatchValueSeparator) ++count;
      }
    }
  }
  return count;
}

/// @brief Returns the PSRAM-backed double-buffer instance NOT currently
///        published on `ctx.bambuMaterialMappings`, allocating both
///        instances on first use. Callers write a freshly parsed table into
///        this instance, then publish it via publishBambuMaterialTable().
/// @param ctx Owning RTOS context.
/// @return Reference to the inactive buffer.
models::BambuMaterialRuleTable& bambuMaterialInactiveBuffer(
    rtos::RtosContext& ctx) {
  static models::BambuMaterialRuleTable* bufferA =
      services::allocatePsramInstance<models::BambuMaterialRuleTable>(
          "StorageTask.bambuMaterialsA");
  static models::BambuMaterialRuleTable* bufferB =
      services::allocatePsramInstance<models::BambuMaterialRuleTable>(
          "StorageTask.bambuMaterialsB");
  const auto* current = ctx.bambuMaterialMappings.load(std::memory_order_relaxed);
  return current == bufferA ? *bufferB : *bufferA;
}

/// @brief Atomically publishes `table` (previously obtained from
///        bambuMaterialInactiveBuffer()) as the active mapping table.
/// @param ctx Owning RTOS context.
/// @param table Newly loaded/validated table to publish.
void publishBambuMaterialTable(rtos::RtosContext& ctx,
                               const models::BambuMaterialRuleTable& table) {
  ctx.bambuMaterialMappings.store(&table, std::memory_order_release);
}

/// @brief Opens, parses and fully validates a bambu_materials.json-shaped
///        file into `out`.
/// @param filesystem Filesystem to read from.
/// @param path Path to open.
/// @param out Table to fill; see services::parseBambuMaterialCatalog()'s
///        contract (may be left partially written on failure).
/// @param result Out parameter receiving the specific success/failure reason.
/// @return true if `out` now holds a fully valid table.
bool loadAndValidateBambuMaterialFile(
    fs::FS& filesystem, const char* path,
    models::BambuMaterialRuleTable& out,
    services::BambuMaterialCatalogResult& result) {
  File file = filesystem.open(path, FILE_READ);
  if (!file || file.isDirectory() || file.size() == 0 ||
      file.size() > config::kBambuMaterialsMaxFileSize) {
    if (file) file.close();
    result.error = services::BambuMaterialCatalogError::InvalidJson;
    result.offendingKey[0] = '\0';
    return false;
  }
  JsonDocument document;
  const DeserializationError parseError = deserializeJson(document, file);
  file.close();
  if (parseError) {
    result.error = services::BambuMaterialCatalogError::InvalidJson;
    result.offendingKey[0] = '\0';
    return false;
  }
  result = services::parseBambuMaterialCatalog(document, out);
  return result.error == services::BambuMaterialCatalogError::Ok;
}

/// @brief Promotes /config/bambu_materials.tmp.json to the active
///        bambu_materials.json, keeping the previous active file as
///        bambu_materials.bak.json -- mirrors services::JsonStorage::
///        atomicSave()'s backup/rename sequence (a separate, small
///        implementation rather than a shared one: this document uses a
///        different schema/envelope than JsonStorage's validator
///        understands, see services/BambuMaterialCatalog.h). Unlike
///        JsonStorage::atomicSave(), the backup is deliberately kept (not
///        removed) after a successful activation -- loadBambuMaterialCatalog()
///        treats it as a valid recovery source if the active file is ever
///        found missing/invalid on a later boot (docs/bambu-protocol.md).
/// @param filesystem Filesystem to operate on.
/// @return true if the temp file is now the active file.
bool activateBambuMaterialFile(fs::FS& filesystem) {
  if (filesystem.exists(config::kBambuMaterialsBackupPath) &&
      !filesystem.remove(config::kBambuMaterialsBackupPath)) {
    return false;
  }
  const bool hadTarget = filesystem.exists(config::kBambuMaterialsPath);
  if (hadTarget &&
      !filesystem.rename(config::kBambuMaterialsPath,
                         config::kBambuMaterialsBackupPath)) {
    return false;
  }
  if (!filesystem.rename(config::kBambuMaterialsTempPath,
                         config::kBambuMaterialsPath)) {
    if (hadTarget) {
      filesystem.rename(config::kBambuMaterialsBackupPath,
                        config::kBambuMaterialsPath);
    }
    return false;
  }
  return true;
}

/// @brief Computes the SHA-256 of a file's content as a lowercase hex string.
/// @param filesystem Filesystem to read from.
/// @param path File to hash.
/// @param outHex65 Destination buffer, at least 65 bytes.
/// @return false if the file could not be opened/read.
bool computeFileSha256Hex(fs::FS& filesystem, const char* path, char* outHex65) {
  File file = filesystem.open(path, FILE_READ);
  if (!file) return false;
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts_ret(&sha, 0);  // 0 = SHA-256, not SHA-224
  std::array<std::uint8_t, 1024> buffer;
  for (;;) {
    const int readBytes = file.read(buffer.data(), buffer.size());
    if (readBytes < 0) {
      file.close();
      mbedtls_sha256_free(&sha);
      return false;
    }
    if (readBytes == 0) break;
    mbedtls_sha256_update_ret(&sha, buffer.data(),
                              static_cast<std::size_t>(readBytes));
  }
  file.close();
  std::uint8_t hash[32];
  mbedtls_sha256_finish_ret(&sha, hash);
  mbedtls_sha256_free(&sha);
  for (std::size_t index = 0; index < sizeof(hash); ++index) {
    std::snprintf(outHex65 + index * 2, 3, "%02x", hash[index]);
  }
  return true;
}

/// @brief Loads /config/bambu_materials.json at boot, with a fallback to a
///        surviving .bak.json (never .tmp.json, which is only ever an
///        in-progress download) -- see docs/bambu-protocol.md. Publishes
///        the loaded table via publishBambuMaterialTable() on success;
///        `ctx.bambuMaterialMappings` is left untouched (nullptr at boot)
///        on failure -- there is deliberately no fallback to a compiled-in
///        table (that defeats the point of moving this to SD, see the
///        Aufgabenbeschreibung this implements).
/// @param ctx Owning RTOS context.
void loadBambuMaterialCatalog(rtos::RtosContext& ctx) {
  const bool targetExists = SD.exists(config::kBambuMaterialsPath);
  const bool tempExists = SD.exists(config::kBambuMaterialsTempPath);
  const bool backupExists = SD.exists(config::kBambuMaterialsBackupPath);
  models::BambuMaterialRuleTable& candidate = bambuMaterialInactiveBuffer(ctx);

  if (targetExists) {
    services::BambuMaterialCatalogResult result{};
    if (loadAndValidateBambuMaterialFile(SD, config::kBambuMaterialsPath,
                                         candidate, result)) {
      if (tempExists) SD.remove(config::kBambuMaterialsTempPath);
      publishBambuMaterialTable(ctx, candidate);
      FS_LOGI(services::LogComponent::Bambu,
              "[BAMBU] Material mappings loaded path=\"%s\" "
              "schema_version=%lu rules=%u match_values=%lu",
              config::kBambuMaterialsPath,
              static_cast<unsigned long>(candidate.schemaVersion),
              static_cast<unsigned>(candidate.ruleCount),
              static_cast<unsigned long>(countBambuMaterialMatchValues(candidate)));
      return;
    }
    FS_LOGW(services::LogComponent::Bambu,
            "[BAMBU] Material mapping load failed path=\"%s\" reason=%s",
            config::kBambuMaterialsPath,
            services::bambuMaterialCatalogErrorName(result.error));
  }

  if (backupExists) {
    services::BambuMaterialCatalogResult result{};
    if (loadAndValidateBambuMaterialFile(SD, config::kBambuMaterialsBackupPath,
                                         candidate, result)) {
      if (!targetExists || SD.remove(config::kBambuMaterialsPath)) {
        if (SD.rename(config::kBambuMaterialsBackupPath,
                      config::kBambuMaterialsPath)) {
          if (tempExists) SD.remove(config::kBambuMaterialsTempPath);
          publishBambuMaterialTable(ctx, candidate);
          FS_LOGI(services::LogComponent::Bambu,
                  "[BAMBU] Material mappings recovered from backup "
                  "path=\"%s\" schema_version=%lu rules=%u match_values=%lu",
                  config::kBambuMaterialsPath,
                  static_cast<unsigned long>(candidate.schemaVersion),
                  static_cast<unsigned>(candidate.ruleCount),
                  static_cast<unsigned long>(
                      countBambuMaterialMatchValues(candidate)));
          return;
        }
      }
      FS_LOGW(services::LogComponent::Bambu,
              "[BAMBU] Material mapping backup recovery failed "
              "reason=rename_failed");
    } else {
      FS_LOGW(services::LogComponent::Bambu,
              "[BAMBU] Material mapping load failed path=\"%s\" reason=%s",
              config::kBambuMaterialsBackupPath,
              services::bambuMaterialCatalogErrorName(result.error));
    }
  }

  // An incomplete/in-progress temp file is never a valid activation source
  // (docs/bambu-protocol.md section 10) -- discard it rather than leaving
  // it to confuse a later manual inspection or download attempt.
  if (tempExists) SD.remove(config::kBambuMaterialsTempPath);
  if (!targetExists && !backupExists) {
    FS_LOGW(services::LogComponent::Bambu,
            "[BAMBU] Material mapping load failed path=\"%s\" "
            "reason=file_missing",
            config::kBambuMaterialsPath);
  }
  // ctx.bambuMaterialMappings stays nullptr -- BambuTask::handleAssignTray()
  // rejects with reason=material_mapping_unavailable until a valid file is
  // loaded (via a later download activation) and this device restarts, or a
  // reload is added -- see docs/bambu-protocol.md.
}

// Shared state for the in-progress Bambu material-mapping download, across
// processBeginBambuMaterialDownload()/processWriteBambuMaterialChunk()/
// processCommitBambuMaterialDownload()/processAbortBambuMaterialDownload()
// below -- file-scope (not function-local) since all four must see the same
// open handle. StorageTask processes exactly one StorageCommand at a time
// (see storageTask()'s single-threaded receive loop), so no locking is
// needed; #kDownloadRequestIdNone marks "no download currently open".
constexpr std::uint32_t kDownloadRequestIdNone = 0;
// Bounds for the short-write retry loop in processWriteBambuMaterialChunk()
// -- generous enough to absorb a momentary SD-card busy condition, bounded
// so a genuinely broken write path still fails within roughly 200 ms.
constexpr std::uint8_t kMaxWriteStallRetries = 10;
constexpr std::uint32_t kWriteStallRetryDelayMs = 20;
File bambuMaterialDownloadFile;                              ///< Open handle to the in-progress download's temp file, or invalid if none is open.
std::uint32_t bambuMaterialDownloadRequestId = kDownloadRequestIdNone;  ///< requestId of the in-progress download, or #kDownloadRequestIdNone.
std::size_t bambuMaterialDownloadBytesWritten = 0;            ///< Bytes written to #bambuMaterialDownloadFile so far.
// Staleness safety net (see storageTask()'s main loop and
// config::kBambuMaterialDownloadStaleTimeoutMs's doc comment) -- set once in
// processBeginBambuMaterialDownload(), not refreshed per chunk (the whole
// file is at most kBambuMaterialsMaxFileSize, realistically a few seconds
// end to end even on a slow card/connection).
TickType_t bambuMaterialDownloadStartedAtTicks = 0;  ///< Tick count when the currently open download started, if any.

/// @brief Closes and discards any in-progress Bambu material-mapping
///        download temp file, resetting the guard state.
void abortBambuMaterialDownloadFile() {
  if (bambuMaterialDownloadFile) bambuMaterialDownloadFile.close();
  if (SD.exists(config::kBambuMaterialsTempPath)) {
    SD.remove(config::kBambuMaterialsTempPath);
  }
  bambuMaterialDownloadRequestId = kDownloadRequestIdNone;
  bambuMaterialDownloadBytesWritten = 0;
}

/// @brief Handles StorageCommandType::BeginBambuMaterialDownload: (re)opens
///        the temp file, discarding any previous in-progress download.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
void processBeginBambuMaterialDownload(rtos::RtosContext& ctx,
                                       const rtos::StorageCommand& command) {
  if (bambuMaterialDownloadRequestId != kDownloadRequestIdNone) {
    FS_LOGW(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download restarted "
            "previous_request_id=%lu new_request_id=%lu",
            static_cast<unsigned long>(bambuMaterialDownloadRequestId),
            static_cast<unsigned long>(command.requestId));
    abortBambuMaterialDownloadFile();
  } else if (SD.exists(config::kBambuMaterialsTempPath)) {
    SD.remove(config::kBambuMaterialsTempPath);
  }

  bambuMaterialDownloadFile = SD.open(config::kBambuMaterialsTempPath, FILE_WRITE);
  if (!bambuMaterialDownloadFile) {
    FS_LOGE(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download failed "
            "temporary=\"%s\" reason=temporary_file_failed",
            config::kBambuMaterialsTempPath);
    sendBambuMaterialUpdateResult(
        ctx, command.requestId, false,
        "Tempor\xC3\xA4re Datei konnte nicht angelegt werden");
    return;
  }
  bambuMaterialDownloadRequestId = command.requestId;
  bambuMaterialDownloadBytesWritten = 0;
  bambuMaterialDownloadStartedAtTicks = xTaskGetTickCount();
  FS_LOGI(services::LogComponent::Bambu,
          "[BAMBU] Material mapping download started target=\"%s\" "
          "temporary=\"%s\" free_heap=%u",
          config::kBambuMaterialsPath, config::kBambuMaterialsTempPath,
          static_cast<unsigned>(ESP.getFreeHeap()));
}

/// @brief Handles StorageCommandType::WriteBambuMaterialChunk: appends
///        `command.json[0..jsonLength)` to the open download temp file.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
/// @note Silently ignored (DEBUG log only) if no download matching
///       `command.requestId` is open -- an expected, harmless race when
///       BeginBambuMaterialDownload already failed and reported its own
///       error; UpdateTask does not learn of that failure (it never reads
///       appEventQueue) and keeps streaming chunks regardless.
void processWriteBambuMaterialChunk(rtos::RtosContext& ctx,
                                    const rtos::StorageCommand& command) {
  if (bambuMaterialDownloadRequestId != command.requestId ||
      !bambuMaterialDownloadFile) {
    FS_LOGD(services::LogComponent::Bambu,
            "[BAMBU] Material mapping chunk ignored request_id=%lu "
            "reason=no_open_download",
            static_cast<unsigned long>(command.requestId));
    return;
  }
  if (command.jsonLength == 0) return;
  if (bambuMaterialDownloadBytesWritten + command.jsonLength >
      config::kBambuMaterialsMaxFileSize) {
    FS_LOGE(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected "
            "reason=file_too_large limit_bytes=%u",
            static_cast<unsigned>(config::kBambuMaterialsMaxFileSize));
    abortBambuMaterialDownloadFile();
    sendBambuMaterialUpdateResult(ctx, command.requestId, false,
                                  "Heruntergeladene Datei ist zu gro\xC3\x9F");
    return;
  }
  // File::write() can legitimately return fewer bytes than requested under
  // a momentary SD-card busy condition (sector commit, wear-leveling) --
  // not a real failure. Retrying the remainder instead of treating any
  // short write as fatal fixed intermittent "written=0"/"written=94"
  // rejections seen on real hardware despite an otherwise healthy download
  // (Nutzerbericht 2026-08-28, TASKS.md). A write that stays stuck at 0
  // bytes for kMaxWriteStallRetries consecutive attempts is still treated
  // as a real failure.
  std::size_t offset = 0;
  std::uint8_t stallRetries = 0;
  while (offset < command.jsonLength) {
    const std::size_t written = bambuMaterialDownloadFile.write(
        reinterpret_cast<const std::uint8_t*>(command.json) + offset,
        command.jsonLength - offset);
    if (written == 0) {
      ++stallRetries;
      if (stallRetries > kMaxWriteStallRetries) {
        bambuMaterialDownloadBytesWritten += offset;
        FS_LOGE(services::LogComponent::Bambu,
                "[BAMBU] Material mapping download rejected "
                "reason=write_failed offset=%u expected=%u",
                static_cast<unsigned>(offset),
                static_cast<unsigned>(command.jsonLength));
        abortBambuMaterialDownloadFile();
        sendBambuMaterialUpdateResult(ctx, command.requestId, false,
                                      "Schreibfehler beim Herunterladen");
        return;
      }
      FS_LOGD(services::LogComponent::Bambu,
              "[BAMBU] Material mapping write stalled offset=%u retry=%u",
              static_cast<unsigned>(offset),
              static_cast<unsigned>(stallRetries));
      vTaskDelay(pdMS_TO_TICKS(kWriteStallRetryDelayMs));
      continue;
    }
    offset += written;
    stallRetries = 0;
  }
  bambuMaterialDownloadBytesWritten += offset;
  FS_LOGD(services::LogComponent::Bambu,
          "[BAMBU] Material mapping chunk written bytes=%u total=%u",
          static_cast<unsigned>(offset),
          static_cast<unsigned>(bambuMaterialDownloadBytesWritten));
}

/// @brief Handles StorageCommandType::AbortBambuMaterialDownload: discards
///        the in-progress temp file (used when UpdateTask hit a
///        network/TLS error mid-stream and already reported its own
///        failure event -- this only cleans up the temp file, no event is
///        sent from here).
/// @param command Command to process.
void processAbortBambuMaterialDownload(const rtos::StorageCommand& command) {
  if (bambuMaterialDownloadRequestId != command.requestId) return;
  FS_LOGI(services::LogComponent::Bambu,
          "[BAMBU] Material mapping download aborted request_id=%lu",
          static_cast<unsigned long>(command.requestId));
  abortBambuMaterialDownloadFile();
}

/// @brief Handles StorageCommandType::CommitBambuMaterialDownload:
///        SHA-256-validates the completed temp file against
///        `command.json` (the expected hash), parses/validates it as a
///        bambu_materials.json document, and -- only if both succeed --
///        atomically activates it and publishes the new RAM cache. This is
///        the sole authority on activation (docs/bambu-protocol.md
///        section 8/9): UpdateTask never writes to the SD card itself.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
void processCommitBambuMaterialDownload(rtos::RtosContext& ctx,
                                        const rtos::StorageCommand& command) {
  if (bambuMaterialDownloadRequestId != command.requestId ||
      !bambuMaterialDownloadFile) {
    FS_LOGD(services::LogComponent::Bambu,
            "[BAMBU] Material mapping commit ignored request_id=%lu "
            "reason=no_open_download",
            static_cast<unsigned long>(command.requestId));
    return;
  }
  // Gives ctx.bambuMaterialDownloadDone on every exit from here on
  // (success or any failure return below), via the destructor -- signals
  // UpdateTask that it may now safely proceed to its own next TLS
  // connection (e.g. the firmware download), see RtosContext.h's doc
  // comment on bambuMaterialDownloadDone for why that ordering matters on
  // real hardware.
  struct DoneSignal {
    SemaphoreHandle_t semaphore;
    ~DoneSignal() { xSemaphoreGive(semaphore); }
  } doneSignal{ctx.bambuMaterialDownloadDone};

  FS_LOGI(services::LogComponent::Bambu,
          "[BAMBU] Material mapping commit received request_id=%lu "
          "bytes_written=%u",
          static_cast<unsigned long>(command.requestId),
          static_cast<unsigned>(bambuMaterialDownloadBytesWritten));

  bambuMaterialDownloadFile.flush();
  const bool writeFailed = bambuMaterialDownloadFile.getWriteError() != 0;
  bambuMaterialDownloadFile.close();
  bambuMaterialDownloadRequestId = kDownloadRequestIdNone;
  if (writeFailed) {
    SD.remove(config::kBambuMaterialsTempPath);
    FS_LOGE(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected reason=write_failed");
    sendBambuMaterialUpdateResult(ctx, command.requestId, false,
                                  "Schreibfehler beim Herunterladen");
    return;
  }

  char expectedHashHex[65]{};
  if (command.jsonLength != 64 ||
      !services::extractHexSha256(command.json, expectedHashHex)) {
    SD.remove(config::kBambuMaterialsTempPath);
    FS_LOGW(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected reason=missing_sha256");
    sendBambuMaterialUpdateResult(
        ctx, command.requestId, false,
        "Keine g\xC3\xBCltige Pr\xC3\xBC" "fsumme \xC3\xBC" "bergeben");
    return;
  }

  char actualHashHex[65]{};
  if (!computeFileSha256Hex(SD, config::kBambuMaterialsTempPath, actualHashHex)) {
    SD.remove(config::kBambuMaterialsTempPath);
    FS_LOGE(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected reason=hash_failed");
    sendBambuMaterialUpdateResult(
        ctx, command.requestId, false,
        "Pr\xC3\xBC" "fsumme konnte nicht berechnet werden");
    return;
  }
  if (std::strcmp(actualHashHex, expectedHashHex) != 0) {
    SD.remove(config::kBambuMaterialsTempPath);
    FS_LOGE(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected reason=sha256_mismatch "
            "expected_sha256=\"%s\" actual_sha256=\"%s\"",
            expectedHashHex, actualHashHex);
    sendBambuMaterialUpdateResult(
        ctx, command.requestId, false,
        "Pr\xC3\xBC" "fsumme stimmt nicht \xC3\xBC" "berein");
    return;
  }
  FS_LOGI(services::LogComponent::Bambu,
          "[BAMBU] Material mapping sha256 validated sha256=\"%s\"",
          actualHashHex);

  models::BambuMaterialRuleTable& candidate = bambuMaterialInactiveBuffer(ctx);
  services::BambuMaterialCatalogResult parseResult{};
  if (!loadAndValidateBambuMaterialFile(SD, config::kBambuMaterialsTempPath,
                                       candidate, parseResult)) {
    SD.remove(config::kBambuMaterialsTempPath);
    FS_LOGW(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected reason=%s",
            services::bambuMaterialCatalogErrorName(parseResult.error));
    sendBambuMaterialUpdateResult(ctx, command.requestId, false,
                                  "Heruntergeladene Datei ist ung\xC3\xBCltig");
    return;
  }
  FS_LOGI(services::LogComponent::Bambu,
          "[BAMBU] Material mapping parsed schema_version=%lu rules=%u "
          "match_values=%lu",
          static_cast<unsigned long>(candidate.schemaVersion),
          static_cast<unsigned>(candidate.ruleCount),
          static_cast<unsigned long>(countBambuMaterialMatchValues(candidate)));

  if (!activateBambuMaterialFile(SD)) {
    SD.remove(config::kBambuMaterialsTempPath);
    FS_LOGE(services::LogComponent::Bambu,
            "[BAMBU] Material mapping download rejected reason=activation_failed");
    sendBambuMaterialUpdateResult(ctx, command.requestId, false,
                                  "Aktivierung fehlgeschlagen");
    return;
  }

  publishBambuMaterialTable(ctx, candidate);
  FS_LOGI(services::LogComponent::Bambu,
          "[BAMBU] Material mapping activated path=\"%s\"",
          config::kBambuMaterialsPath);
  sendBambuMaterialUpdateResult(ctx, command.requestId, true,
                                "Material-Zuordnung aktualisiert");
}

/// @brief Validates the path and dispatches to processLoadCommand()/
///        processSaveCommand()/direct delete, based on `command.type`.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
/// @note The four BambuMaterial* download commands are dispatched first,
///       before the generic path check below -- they never use
///       `command.path` (the temp path is a fixed constant, see
///       config/BambuMaterialConfig.h), by design: trusting a caller-
///       supplied path for this fixed, security-relevant temp file would
///       reopen exactly the path-injection risk isAllowedJsonPath() exists
///       to prevent for every other command.
void processStorageCommand(rtos::RtosContext& ctx,
                           const rtos::StorageCommand& command) {
  switch (command.type) {
    case rtos::StorageCommandType::BeginBambuMaterialDownload:
      processBeginBambuMaterialDownload(ctx, command);
      return;
    case rtos::StorageCommandType::WriteBambuMaterialChunk:
      processWriteBambuMaterialChunk(ctx, command);
      return;
    case rtos::StorageCommandType::CommitBambuMaterialDownload:
      processCommitBambuMaterialDownload(ctx, command);
      return;
    case rtos::StorageCommandType::AbortBambuMaterialDownload:
      processAbortBambuMaterialDownload(command);
      return;
    case rtos::StorageCommandType::LoadJson:
    case rtos::StorageCommandType::SaveJson:
    case rtos::StorageCommandType::DeleteJson:
      break;
  }

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
      if (!SD.exists(command.path) || SD.remove(command.path)) {
        sendStorageEvent(ctx, rtos::AppEventType::StorageWriteCompleted,
                         "JSON deleted", command.requestId);
      } else {
        sendStorageEvent(ctx, rtos::AppEventType::StorageRequestError,
                         "JSON delete failed", command.requestId);
      }
      return;
    default:
      return;
  }
}

/// @brief Cheap SD-card presence/health check: card type plus a root-directory open.
/// @return true if the card responds and its root directory can be opened.
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

/// @brief Ensures a directory exists, creating it if necessary.
/// @param path Directory path.
/// @return true if the path exists (or was created) and is a directory.
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

/// @brief Ensures every directory in #kRequiredDirectories exists.
/// @return true if all directories are ready.
bool ensureDirectoryStructure() {
  for (const char* path : kRequiredDirectories) {
    if (!ensureDirectory(path)) {
      FS_LOGE(services::LogComponent::Storage,
              "Required directory unavailable path=%s", path);
      return false;
    }
  }
  return true;
}

/// @brief Recovers/validates/creates one initial config document at boot.
/// @param definition Document path and type to ensure.
/// @return true if the document ends up present and valid (or, for a
///         damaged legacy mapping file, is left untouched for diagnosis
///         rather than failing the whole subsystem).
bool ensureInitialDocument(const InitialDocument& definition) {
  const services::JsonStorageResult recovery =
      services::JsonStorage::recoverAtomicSave(SD, definition.path,
                                               definition.type);
  if (!recovery.ok()) {
    FS_LOGE(services::LogComponent::Storage,
            "Document recovery failed path=%s error=%s", definition.path,
            services::JsonStorage::errorName(recovery.error));
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
      FS_LOGE(services::LogComponent::Storage,
              "Initial document invalid path=%s error=%s", definition.path,
              services::JsonStorage::errorName(loaded.error));
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
    FS_LOGE(services::LogComponent::Storage,
            "Initial document creation failed path=%s error=%s",
            definition.path, services::JsonStorage::errorName(saved.error));
    return false;
  }
  FS_LOGI(services::LogComponent::Storage, "Initial document created path=%s",
          definition.path);
  return true;
}

/// @brief Ensures every document in #kInitialDocuments is present and valid.
/// @return true if all documents are ready.
bool ensureInitialDocuments() {
  for (const InitialDocument& definition : kInitialDocuments) {
    if (!ensureInitialDocument(definition)) {
      return false;
    }
  }
  return true;
}

/// @brief Text name for an SD.cardType() value, used in log lines.
/// @param cardType Card type constant (CARD_MMC/CARD_SD/CARD_SDHC/CARD_NONE).
/// @return Static, NUL-terminated name.
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

/// @brief Logs SD card type, capacity, and filesystem usage at startup.
void logSdCardInfo() {
  const std::uint64_t cardSize = SD.cardSize();
  const std::uint64_t totalBytes = SD.totalBytes();
  const std::uint64_t usedBytes = SD.usedBytes();
  const std::uint64_t freeBytes =
      totalBytes >= usedBytes ? totalBytes - usedBytes : 0;
  constexpr std::uint64_t kBytesPerMiB = 1024ULL * 1024ULL;

  FS_LOGI(services::LogComponent::Storage,
          "SD mounted type=\"%s\" capacity_bytes=%llu capacity_mib=%llu",
          cardTypeName(SD.cardType()), static_cast<unsigned long long>(cardSize),
          static_cast<unsigned long long>(cardSize / kBytesPerMiB));
  FS_LOGI(services::LogComponent::Storage,
          "Filesystem usage total_bytes=%llu used_bytes=%llu free_bytes=%llu",
          static_cast<unsigned long long>(totalBytes),
          static_cast<unsigned long long>(usedBytes),
          static_cast<unsigned long long>(freeBytes));
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
    FS_LOGE(services::LogComponent::Storage,
            "Initialization failed stage=%s",
            !mounted ? "sd_mount"
                     : structureReady ? "initial_documents" : "directories");
  } else {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    logSdCardInfo();
    sendStorageEvent(ctx, rtos::AppEventType::SdMounted,
                     "SD card, directories and configuration ready");
    FS_LOGI(services::LogComponent::Storage,
            "Initial JSON documents ready count=%u",
            static_cast<unsigned>(sizeof(kInitialDocuments) /
                                  sizeof(kInitialDocuments[0])));
    // Not an "initial document" (see #kInitialDocuments): no default is
    // auto-created if missing (docs/bambu-protocol.md) -- a missing/invalid
    // file leaves ctx.bambuMaterialMappings nullptr rather than failing SD
    // initialization as a whole.
    loadBambuMaterialCatalog(ctx);
    FS_LOGD(services::LogComponent::Storage,
            "Stack watermark free_bytes=%u",
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  }

  bool removalLatched =
      (xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_SD_READY) == 0;
  bool reinsertionReported = false;
  // Der feste JSON-Puffer macht StorageCommand für eine Stackvariable zu
  // gross. Der Puffer bleibt statisch und gehört weiterhin exklusiv diesem
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
        FS_LOGW(services::LogComponent::Storage,
                "Command rejected reason=restart_required request_id=%lu",
                static_cast<unsigned long>(command.requestId));
      } else {
        const std::uint32_t startMs = millis();
        processStorageCommand(ctx, command);
        const std::uint32_t elapsedMs = millis() - startMs;
        if (elapsedMs >= config::kSdSlowOperationWarningMs) {
          FS_LOGW(services::LogComponent::Storage,
                  "Slow SD operation request_id=%lu type=%u path=\"%s\" "
                  "elapsed_ms=%lu",
                  static_cast<unsigned long>(command.requestId),
                  static_cast<unsigned>(command.type), command.path,
                  static_cast<unsigned long>(elapsedMs));
        }
      }
    }

    // Staleness safety net (Nutzerbericht 2026-08-28: unbootable device, SD
    // card needed repair) -- an open download temp file that never receives
    // its Commit/Abort (e.g. UpdateTask crashing/rebooting between Begin and
    // Commit) would otherwise stay open for the rest of this boot session.
    // Force-closed here instead of waiting indefinitely; see
    // config::kBambuMaterialDownloadStaleTimeoutMs's doc comment.
    if (bambuMaterialDownloadRequestId != kDownloadRequestIdNone &&
        pdTICKS_TO_MS(static_cast<TickType_t>(
            xTaskGetTickCount() - bambuMaterialDownloadStartedAtTicks)) >=
            config::kBambuMaterialDownloadStaleTimeoutMs) {
      FS_LOGE(services::LogComponent::Bambu,
              "[BAMBU] Material mapping download stale, force-closing "
              "request_id=%lu bytes_written=%u",
              static_cast<unsigned long>(bambuMaterialDownloadRequestId),
              static_cast<unsigned>(bambuMaterialDownloadBytesWritten));
      abortBambuMaterialDownloadFile();
    }

    // Skipped while a Bambu material-mapping download is actively writing
    // its temp file (TASKS.md Nachtrag 2026-08-28, Nutzerbericht):
    // cardIsAccessible() opens/closes a second File handle ("/") on every
    // single loop iteration -- interleaved with the many rapid
    // WriteBambuMaterialChunk writes to the still-open download file, this
    // reliably corrupted one of the writes ("reason=write_failed
    // written=0") a few chunks in. No such interleaving happens for the
    // occasional single LoadJson/SaveJson command this check was designed
    // around, so skipping it only during this specific multi-part
    // operation is safe -- SD removal is still caught as soon as the
    // download finishes (success, failure, or abort) and this loop resumes
    // its normal per-iteration check.
    const bool accessible = bambuMaterialDownloadRequestId != kDownloadRequestIdNone
                                ? true
                                : cardIsAccessible();
    if (!removalLatched && !accessible) {
      removalLatched = true;
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
      sendStorageEvent(ctx, rtos::AppEventType::SdRemoved,
                       "SD card removed; restart required");
      FS_LOGE(services::LogComponent::Storage,
              "SD removed restart_required=true");
    } else if (removalLatched && accessible && !reinsertionReported) {
      reinsertionReported = true;
      sendStorageEvent(ctx, rtos::AppEventType::SdReinserted,
                       "SD card reinserted; restart still required");
      FS_LOGW(services::LogComponent::Storage,
              "SD reinserted restart_required=true");
    }
  }
}
}  // namespace filament_station::tasks
