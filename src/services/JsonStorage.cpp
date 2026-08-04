#include "services/JsonStorage.h"

#include <cctype>
#include <cstring>

namespace filament_station::services {
namespace {

constexpr std::size_t kSmallConfigMaxSize = 4U * 1024U;
constexpr std::size_t kConfigMaxSize = 8U * 1024U;
constexpr std::size_t kLargeConfigMaxSize = 16U * 1024U;
constexpr std::size_t kDiagnosticsMaxSize = 32U * 1024U;

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
  if (error == JsonStorageError::Ok) {
    error = validate(document);
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

JsonStorageResult JsonStorage::serialize(
    const JsonDocument& document, rtos::StorageDocumentType documentType,
    Print& output) {
  const JsonStorageError validationError = validate(document);
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

const char* JsonStorage::errorName(JsonStorageError error) {
  switch (error) {
    case JsonStorageError::Ok:
      return "ok";
    case JsonStorageError::InvalidArgument:
      return "invalid_argument";
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
    case JsonStorageError::OutputTooLarge:
      return "output_too_large";
    case JsonStorageError::SerializeFailed:
      return "serialize_failed";
  }
  return "unknown";
}

}  // namespace filament_station::services
