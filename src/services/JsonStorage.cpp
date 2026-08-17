#include "services/JsonStorage.h"

#include <cctype>
#include <cstring>

namespace filament_station::services {
namespace {

constexpr std::size_t kSmallConfigMaxSize = 4U * 1024U;
constexpr std::size_t kConfigMaxSize = 8U * 1024U;
constexpr std::size_t kLargeConfigMaxSize = 16U * 1024U;
constexpr std::size_t kDiagnosticsMaxSize = 32U * 1024U;
constexpr std::size_t kAtomicPathCapacity = 112U;

struct AtomicPaths {
  char temporary[kAtomicPathCapacity]{};
  char backup[kAtomicPathCapacity]{};
};

bool makeAtomicPaths(const char* targetPath, AtomicPaths& paths) {
  if (targetPath == nullptr || targetPath[0] != '/') {
    return false;
  }

  constexpr char kJsonSuffix[] = ".json";
  constexpr char kTemporarySuffix[] = ".tmp.json";
  constexpr char kBackupSuffix[] = ".bak.json";
  const std::size_t targetLength = std::strlen(targetPath);
  const std::size_t jsonSuffixLength = sizeof(kJsonSuffix) - 1U;
  if (targetLength <= jsonSuffixLength ||
      std::strcmp(targetPath + targetLength - jsonSuffixLength,
                  kJsonSuffix) != 0) {
    return false;
  }

  const std::size_t stemLength = targetLength - jsonSuffixLength;
  if (stemLength + sizeof(kTemporarySuffix) > sizeof(paths.temporary) ||
      stemLength + sizeof(kBackupSuffix) > sizeof(paths.backup)) {
    return false;
  }

  std::memcpy(paths.temporary, targetPath, stemLength);
  std::memcpy(paths.temporary + stemLength, kTemporarySuffix,
              sizeof(kTemporarySuffix));
  std::memcpy(paths.backup, targetPath, stemLength);
  std::memcpy(paths.backup + stemLength, kBackupSuffix,
              sizeof(kBackupSuffix));
  return true;
}

bool removeIfPresent(fs::FS& filesystem, const char* path) {
  return !filesystem.exists(path) || filesystem.remove(path);
}

bool validMappingFormat(const char* format) {
  return format != nullptr &&
         (std::strcmp(format, "bambuLab") == 0 ||
          std::strcmp(format, "openPrintTag") == 0 ||
          std::strcmp(format, "openTag3D") == 0 ||
          std::strcmp(format, "legacy") == 0 ||
          std::strcmp(format, "unknown") == 0);
}

bool validNormalizedUid(const char* uid) {
  if (uid == nullptr) return false;
  const std::size_t length = std::strlen(uid);
  if (length < 8 || length > 20 || (length & 1U) != 0) return false;
  for (std::size_t index = 0; index < length; ++index) {
    if (!std::isdigit(static_cast<unsigned char>(uid[index])) &&
        (uid[index] < 'A' || uid[index] > 'F'))
      return false;
  }
  return true;
}

bool isValidDocumentFile(fs::FS& filesystem, const char* path,
                         rtos::StorageDocumentType documentType) {
  File file = filesystem.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  JsonDocument document;
  const JsonStorageResult result =
      JsonStorage::load(file, documentType, document);
  file.close();
  return result.ok();
}

bool replaceWith(fs::FS& filesystem, const char* source,
                 const char* destination) {
  return removeIfPresent(filesystem, destination) &&
         filesystem.rename(source, destination);
}

bool isDigit(char value) {
  return std::isdigit(static_cast<unsigned char>(value)) != 0;
}

bool isUtcTimestamp(const char* value) {
  if (value == nullptr || std::strlen(value) != 20U) {
    return false;
  }

  constexpr std::size_t kDigitPositions[] = {
      0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
  for (const std::size_t position : kDigitPositions) {
    if (!isDigit(value[position])) {
      return false;
    }
  }

  return value[4] == '-' && value[7] == '-' && value[10] == 'T' &&
         value[13] == ':' && value[16] == ':' && value[19] == 'Z';
}

bool isNonEmptyString(const JsonVariantConst value) {
  return value.is<const char*>() && value.as<const char*>()[0] != '\0';
}

}  // namespace

std::size_t JsonStorage::maxSizeFor(
    rtos::StorageDocumentType documentType) {
  switch (documentType) {
    case rtos::StorageDocumentType::Scale:
      return kSmallConfigMaxSize;
    case rtos::StorageDocumentType::Device:
    case rtos::StorageDocumentType::Network:
    case rtos::StorageDocumentType::Spoolman:
    case rtos::StorageDocumentType::Bambu:
      return kConfigMaxSize;
    case rtos::StorageDocumentType::Ui:
    case rtos::StorageDocumentType::Nfc:
      return kLargeConfigMaxSize;
    case rtos::StorageDocumentType::Diagnostics:
      return kDiagnosticsMaxSize;
  }
  return 0;
}

JsonStorageResult JsonStorage::load(
    File& file, rtos::StorageDocumentType documentType,
    JsonDocument& document) {
  if (!file || file.isDirectory()) {
    return {JsonStorageError::FileUnavailable, 0};
  }

  const std::size_t maximumSize = maxSizeFor(documentType);
  if (maximumSize == 0) {
    return {JsonStorageError::InvalidArgument, 0};
  }

  const std::size_t fileSize = file.size();
  if (fileSize == 0) {
    return {JsonStorageError::EmptyDocument, 0};
  }
  if (fileSize > maximumSize) {
    return {JsonStorageError::FileTooLarge, 0};
  }
  if (!file.seek(0)) {
    return {JsonStorageError::ReadFailed, 0};
  }

  document.clear();
  const DeserializationError parseError = deserializeJson(document, file);
  if (parseError) {
    document.clear();
    return {JsonStorageError::ParseFailed, fileSize};
  }

  JsonStorageError error = applyDefaults(document);
  if (error == JsonStorageError::Ok &&
      documentType == rtos::StorageDocumentType::Scale) {
    if (document["tareOffsetCounts"].isNull()) {
      document["tareOffsetCounts"] = 0;
    }
    if (document["factorCountsPerGram"].isNull()) {
      document["factorCountsPerGram"] = 1.0F;
    }
  }
  if (error == JsonStorageError::Ok) {
    error = validate(document, documentType);
  }
  if (error != JsonStorageError::Ok) {
    document.clear();
  }
  return {error, fileSize};
}

JsonStorageError JsonStorage::applyDefaults(JsonDocument& document) {
  if (!document.is<JsonObject>()) {
    return JsonStorageError::RootNotObject;
  }

  if (document["schemaVersion"].isNull()) {
    document["schemaVersion"] = kCurrentJsonSchemaVersion;
  }
  if (document["updatedAt"].isNull()) {
    document["updatedAt"] = kDefaultUpdatedAt;
  }
  return JsonStorageError::Ok;
}

JsonStorageError JsonStorage::validate(const JsonDocument& document) {
  if (!document.is<JsonObjectConst>()) {
    return JsonStorageError::RootNotObject;
  }
  if (!document["schemaVersion"].is<std::uint32_t>()) {
    return JsonStorageError::InvalidSchemaVersion;
  }

  const std::uint32_t schemaVersion = document["schemaVersion"];
  if (schemaVersion != kCurrentJsonSchemaVersion) {
    return JsonStorageError::UnsupportedSchemaVersion;
  }
  if (!document["updatedAt"].is<const char*>() ||
      !isUtcTimestamp(document["updatedAt"].as<const char*>())) {
    return JsonStorageError::InvalidUpdatedAt;
  }
  return JsonStorageError::Ok;
}

const char* JsonStorage::documentTypeName(
    rtos::StorageDocumentType documentType) {
  switch (documentType) {
    case rtos::StorageDocumentType::Device:
      return "device";
    case rtos::StorageDocumentType::Network:
      return "network";
    case rtos::StorageDocumentType::Spoolman:
      return "spoolman";
    case rtos::StorageDocumentType::Bambu:
      return "bambu";
    case rtos::StorageDocumentType::Ui:
      return "ui";
    case rtos::StorageDocumentType::Scale:
      return "scale";
    case rtos::StorageDocumentType::Nfc:
      return "nfc";
    case rtos::StorageDocumentType::Diagnostics:
      return "diagnostics";
  }
  return nullptr;
}

JsonStorageError JsonStorage::createDefault(
    rtos::StorageDocumentType documentType, JsonDocument& document) {
  const char* typeName = documentTypeName(documentType);
  if (typeName == nullptr) {
    return JsonStorageError::InvalidArgument;
  }
  document.clear();
  document.to<JsonObject>();
  document["schemaVersion"] = kCurrentJsonSchemaVersion;
  document["updatedAt"] = kDefaultUpdatedAt;
  document["documentType"] = typeName;

  switch (documentType) {
    case rtos::StorageDocumentType::Device:
      document["deviceName"] = "FilamentStation";
      break;
    case rtos::StorageDocumentType::Network:
      document["hostname"] = "filamentstation";
      document["dhcp"] = true;
      break;
    case rtos::StorageDocumentType::Spoolman:
      document["enabled"] = false;
      document["serverUrl"] = "";
      break;
    case rtos::StorageDocumentType::Bambu:
      document["selectedPrinterId"] = 0;
      document["defaultPrinterId"] = 0;
      document["printers"].to<JsonArray>();
      break;
    case rtos::StorageDocumentType::Ui:
      document["language"] = "de";
      document["weightUnit"] = "g";
      break;
    case rtos::StorageDocumentType::Scale:
      document["calibrated"] = false;
      document["tareOffsetCounts"] = 0;
      document["factorCountsPerGram"] = 1.0F;
      break;
    case rtos::StorageDocumentType::Nfc:
      document["tagSchemaVersion"] = 1;
      document["mappings"].to<JsonArray>();
      break;
    case rtos::StorageDocumentType::Diagnostics:
      break;
  }
  return validate(document, documentType);
}

JsonStorageError JsonStorage::validate(
    const JsonDocument& document,
    rtos::StorageDocumentType documentType) {
  JsonStorageError error = validate(document);
  if (error != JsonStorageError::Ok) {
    return error;
  }
  const char* expectedType = documentTypeName(documentType);
  if (expectedType == nullptr || !document["documentType"].is<const char*>() ||
      std::strcmp(document["documentType"].as<const char*>(), expectedType) !=
          0) {
    return JsonStorageError::InvalidDocumentType;
  }

  switch (documentType) {
    case rtos::StorageDocumentType::Device:
      return isNonEmptyString(document["deviceName"])
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Network:
      return isNonEmptyString(document["hostname"]) &&
                     document["dhcp"].is<bool>()
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Spoolman:
      return document["enabled"].is<bool>() &&
                     document["serverUrl"].is<const char*>() &&
                     (!document["enabled"].as<bool>() ||
                      isNonEmptyString(document["serverUrl"]))
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Ui:
      return isNonEmptyString(document["language"]) &&
                     isNonEmptyString(document["weightUnit"])
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Scale:
      if (!document["calibrated"].is<bool>() ||
          !document["tareOffsetCounts"].is<std::int32_t>() ||
          !document["factorCountsPerGram"].is<float>()) {
        return JsonStorageError::InvalidDocumentField;
      }
      if (document["calibrated"].as<bool>() &&
          document["factorCountsPerGram"].as<float>() == 0.0F) {
        return JsonStorageError::InvalidDocumentField;
      }
      return JsonStorageError::Ok;
    case rtos::StorageDocumentType::Nfc:
      if (!document["tagSchemaVersion"].is<std::uint32_t>() ||
          document["tagSchemaVersion"].as<std::uint32_t>() != 1U)
        return JsonStorageError::InvalidDocumentField;
      // /config/nfc.json and the mapping files intentionally share the NFC
      // document type.  Only mapping documents contain this array; their
      // path-specific mandatory validation is performed by StorageTask.
      if (document["mappings"].isNull()) return JsonStorageError::Ok;
      if (!document["mappings"].is<JsonArrayConst>())
        return JsonStorageError::InvalidDocumentField;
      {
        const JsonArrayConst mappings = document["mappings"].as<JsonArrayConst>();
        for (std::size_t index = 0; index < mappings.size(); ++index) {
          const JsonObjectConst mapping = mappings[index].as<JsonObjectConst>();
          const char* uid = mapping["uid"] | static_cast<const char*>(nullptr);
          const char* format =
              mapping["format"] | static_cast<const char*>(nullptr);
          if (!validNormalizedUid(uid) ||
              !mapping["spoolId"].is<std::uint32_t>() ||
              mapping["spoolId"].as<std::uint32_t>() == 0 ||
              (format != nullptr && !validMappingFormat(format)))
            return JsonStorageError::InvalidDocumentField;
          for (std::size_t other = 0; other < index; ++other) {
            const char* otherUid =
                mappings[other]["uid"] | static_cast<const char*>(nullptr);
            if (otherUid != nullptr && std::strcmp(uid, otherUid) == 0)
              return JsonStorageError::InvalidDocumentField;
          }
        }
      }
      return JsonStorageError::Ok;
    case rtos::StorageDocumentType::Bambu:
      return document["selectedPrinterId"].is<std::uint16_t>() &&
                     document["defaultPrinterId"].is<std::uint16_t>() &&
                     document["printers"].is<JsonArrayConst>()
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Diagnostics:
      return JsonStorageError::Ok;
  }
  return JsonStorageError::InvalidArgument;
}

JsonStorageResult JsonStorage::serialize(
    const JsonDocument& document, rtos::StorageDocumentType documentType,
    Print& output) {
  const JsonStorageError validationError = validate(document, documentType);
  if (validationError != JsonStorageError::Ok) {
    return {validationError, 0};
  }

  const std::size_t maximumSize = maxSizeFor(documentType);
  if (maximumSize == 0) {
    return {JsonStorageError::InvalidArgument, 0};
  }

  const std::size_t requiredSize = measureJson(document);
  if (requiredSize == 0) {
    return {JsonStorageError::SerializeFailed, 0};
  }
  if (requiredSize > maximumSize) {
    return {JsonStorageError::OutputTooLarge, 0};
  }

  const std::size_t written = serializeJson(document, output);
  if (written != requiredSize) {
    return {JsonStorageError::SerializeFailed, written};
  }
  return {JsonStorageError::Ok, written};
}

JsonStorageResult JsonStorage::recoverAtomicSave(
    fs::FS& filesystem, const char* targetPath,
    rtos::StorageDocumentType documentType) {
  AtomicPaths paths{};
  if (!makeAtomicPaths(targetPath, paths)) {
    return {JsonStorageError::InvalidPath, 0};
  }

  const bool targetExists = filesystem.exists(targetPath);
  const bool temporaryExists = filesystem.exists(paths.temporary);
  const bool backupExists = filesystem.exists(paths.backup);
  const bool targetValid =
      targetExists && isValidDocumentFile(filesystem, targetPath, documentType);

  if (targetValid) {
    if (!removeIfPresent(filesystem, paths.temporary) ||
        !removeIfPresent(filesystem, paths.backup)) {
      return {JsonStorageError::RecoveryFailed, 0};
    }
    return {JsonStorageError::Ok, 0};
  }

  const bool temporaryValid = temporaryExists &&
                              isValidDocumentFile(filesystem, paths.temporary,
                                                  documentType);
  const bool backupValid = backupExists &&
                           isValidDocumentFile(filesystem, paths.backup,
                                               documentType);

  if (temporaryValid) {
    if (!removeIfPresent(filesystem, targetPath) ||
        !filesystem.rename(paths.temporary, targetPath) ||
        !isValidDocumentFile(filesystem, targetPath, documentType) ||
        !removeIfPresent(filesystem, paths.backup)) {
      return {JsonStorageError::RecoveryFailed, 0};
    }
    return {JsonStorageError::Ok, 0};
  }

  if (backupValid) {
    if (!removeIfPresent(filesystem, targetPath) ||
        !filesystem.rename(paths.backup, targetPath) ||
        !isValidDocumentFile(filesystem, targetPath, documentType) ||
        !removeIfPresent(filesystem, paths.temporary)) {
      return {JsonStorageError::RecoveryFailed, 0};
    }
    return {JsonStorageError::Ok, 0};
  }

  if (!targetExists && !temporaryExists && !backupExists) {
    return {JsonStorageError::Ok, 0};
  }
  return {JsonStorageError::RecoveryFailed, 0};
}

JsonStorageResult JsonStorage::atomicSave(
    fs::FS& filesystem, const char* targetPath,
    rtos::StorageDocumentType documentType, const JsonDocument& document) {
  AtomicPaths paths{};
  if (!makeAtomicPaths(targetPath, paths)) {
    return {JsonStorageError::InvalidPath, 0};
  }

  const JsonStorageError validationError = validate(document, documentType);
  if (validationError != JsonStorageError::Ok) {
    return {validationError, 0};
  }
  if (filesystem.exists(paths.temporary) || filesystem.exists(paths.backup)) {
    const JsonStorageResult recoveryResult =
        recoverAtomicSave(filesystem, targetPath, documentType);
    if (!recoveryResult.ok()) {
      return recoveryResult;
    }
  }
  if (!removeIfPresent(filesystem, paths.temporary)) {
    return {JsonStorageError::TemporaryFileFailed, 0};
  }

  File temporaryFile = filesystem.open(paths.temporary, FILE_WRITE);
  if (!temporaryFile) {
    return {JsonStorageError::TemporaryFileFailed, 0};
  }
  JsonStorageResult writeResult =
      serialize(document, documentType, temporaryFile);
  temporaryFile.flush();
  const bool writeFailed = temporaryFile.getWriteError() != 0;
  temporaryFile.close();
  if (!writeResult.ok() || writeFailed) {
    removeIfPresent(filesystem, paths.temporary);
    return {writeResult.ok() ? JsonStorageError::TemporaryFileFailed
                             : writeResult.error,
            writeResult.bytesProcessed};
  }

  if (!isValidDocumentFile(filesystem, paths.temporary, documentType)) {
    removeIfPresent(filesystem, paths.temporary);
    return {JsonStorageError::TemporaryValidationFailed,
            writeResult.bytesProcessed};
  }
  if (!removeIfPresent(filesystem, paths.backup)) {
    return {JsonStorageError::BackupFailed, writeResult.bytesProcessed};
  }

  const bool hadTarget = filesystem.exists(targetPath);
  if (hadTarget && !filesystem.rename(targetPath, paths.backup)) {
    return {JsonStorageError::BackupFailed, writeResult.bytesProcessed};
  }
  if (!filesystem.rename(paths.temporary, targetPath)) {
    if (hadTarget) {
      replaceWith(filesystem, paths.backup, targetPath);
    }
    return {JsonStorageError::CommitFailed, writeResult.bytesProcessed};
  }
  if (!isValidDocumentFile(filesystem, targetPath, documentType)) {
    if (hadTarget) {
      replaceWith(filesystem, paths.backup, targetPath);
    }
    return {JsonStorageError::CommitFailed, writeResult.bytesProcessed};
  }
  if (!removeIfPresent(filesystem, paths.backup)) {
    return {JsonStorageError::BackupFailed, writeResult.bytesProcessed};
  }
  return writeResult;
}

const char* JsonStorage::errorName(JsonStorageError error) {
  switch (error) {
    case JsonStorageError::Ok:
      return "ok";
    case JsonStorageError::InvalidArgument:
      return "invalid_argument";
    case JsonStorageError::InvalidPath:
      return "invalid_path";
    case JsonStorageError::FileUnavailable:
      return "file_unavailable";
    case JsonStorageError::EmptyDocument:
      return "empty_document";
    case JsonStorageError::FileTooLarge:
      return "file_too_large";
    case JsonStorageError::ReadFailed:
      return "read_failed";
    case JsonStorageError::ParseFailed:
      return "parse_failed";
    case JsonStorageError::RootNotObject:
      return "root_not_object";
    case JsonStorageError::InvalidSchemaVersion:
      return "invalid_schema_version";
    case JsonStorageError::UnsupportedSchemaVersion:
      return "unsupported_schema_version";
    case JsonStorageError::InvalidUpdatedAt:
      return "invalid_updated_at";
    case JsonStorageError::InvalidDocumentType:
      return "invalid_document_type";
    case JsonStorageError::InvalidDocumentField:
      return "invalid_document_field";
    case JsonStorageError::OutputTooLarge:
      return "output_too_large";
    case JsonStorageError::SerializeFailed:
      return "serialize_failed";
    case JsonStorageError::TemporaryFileFailed:
      return "temporary_file_failed";
    case JsonStorageError::TemporaryValidationFailed:
      return "temporary_validation_failed";
    case JsonStorageError::BackupFailed:
      return "backup_failed";
    case JsonStorageError::CommitFailed:
      return "commit_failed";
    case JsonStorageError::RecoveryFailed:
      return "recovery_failed";
  }
  return "unknown";
}

}  // namespace filament_station::services
