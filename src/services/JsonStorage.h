#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <Print.h>
#include <cstddef>
#include <cstdint>

#include "rtos/Messages.h"

namespace filament_station::services {

constexpr std::uint32_t kCurrentJsonSchemaVersion = 1;
constexpr char kDefaultUpdatedAt[] = "1970-01-01T00:00:00Z";

enum class JsonStorageError : std::uint8_t {
  Ok,
  InvalidArgument,
  InvalidPath,
  FileUnavailable,
  EmptyDocument,
  FileTooLarge,
  ReadFailed,
  ParseFailed,
  RootNotObject,
  InvalidSchemaVersion,
  UnsupportedSchemaVersion,
  InvalidUpdatedAt,
  OutputTooLarge,
  SerializeFailed,
  TemporaryFileFailed,
  TemporaryValidationFailed,
  BackupFailed,
  CommitFailed,
  RecoveryFailed
};

struct JsonStorageResult {
  JsonStorageError error = JsonStorageError::Ok;
  std::size_t bytesProcessed = 0;

  constexpr bool ok() const { return error == JsonStorageError::Ok; }
};

class JsonStorage {
 public:
  static std::size_t maxSizeFor(rtos::StorageDocumentType documentType);
  static JsonStorageResult load(File& file,
                                rtos::StorageDocumentType documentType,
                                JsonDocument& document);
  static JsonStorageError applyDefaults(JsonDocument& document);
  static JsonStorageError validate(const JsonDocument& document);
  static JsonStorageResult serialize(
      const JsonDocument& document,
      rtos::StorageDocumentType documentType, Print& output);
  static JsonStorageResult atomicSave(
      fs::FS& filesystem, const char* targetPath,
      rtos::StorageDocumentType documentType, const JsonDocument& document);
  static JsonStorageResult recoverAtomicSave(
      fs::FS& filesystem, const char* targetPath,
      rtos::StorageDocumentType documentType);
  static const char* errorName(JsonStorageError error);
};

}  // namespace filament_station::services
