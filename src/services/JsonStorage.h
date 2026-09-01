/**
 * @file
 * @brief Schema validation, defaulting, and crash-safe atomic read/write
 *        for every persisted JSON document (config, caches, mapping
 *        files). The atomic-save scheme (temp -> validate -> backup ->
 *        commit) survives power loss mid-write; recoverAtomicSave()
 *        restores a consistent state on the next boot.
 */
#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <Print.h>
#include <cstddef>
#include <cstdint>

#include "rtos/Messages.h"

namespace filament_station::services {

constexpr std::uint32_t kCurrentJsonSchemaVersion = 1;  ///< Schema version stamped into every document; documents with any other value are rejected.
constexpr char kDefaultUpdatedAt[] = "1970-01-01T00:00:00Z";  ///< Placeholder "updatedAt" timestamp used when creating a fresh default document.

/// @brief Failure classification returned by every JsonStorage operation.
enum class JsonStorageError : std::uint8_t {
  Ok,                          ///< No error.
  InvalidArgument,             ///< An argument (e.g. an unrecognized document type) was invalid.
  InvalidPath,                 ///< `targetPath` is not a valid "/....json" path atomicSave()/recoverAtomicSave() can derive temp/backup paths from.
  FileUnavailable,             ///< The file could not be opened, or is a directory.
  EmptyDocument,                ///< The file is zero bytes.
  FileTooLarge,                 ///< The file exceeds maxSizeFor() for its document type.
  ReadFailed,                   ///< Seeking/reading the file failed.
  ParseFailed,                  ///< The file's content is not valid JSON.
  RootNotObject,                ///< The document's root is not a JSON object.
  InvalidSchemaVersion,         ///< "schemaVersion" is missing or not a number.
  UnsupportedSchemaVersion,     ///< "schemaVersion" does not match kCurrentJsonSchemaVersion.
  InvalidUpdatedAt,             ///< "updatedAt" is missing or not a valid UTC timestamp.
  InvalidDocumentType,          ///< "documentType" does not match the expected value for this StorageDocumentType.
  InvalidDocumentField,         ///< A document-type-specific required field is missing or fails validation.
  OutputTooLarge,                ///< The serialized document would exceed maxSizeFor() for its document type.
  SerializeFailed,               ///< Serialization produced a different byte count than measureJson() predicted.
  TemporaryFileFailed,           ///< The ".tmp.json" file could not be created or written.
  TemporaryValidationFailed,     ///< The written ".tmp.json" file failed to re-parse/validate.
  BackupFailed,                  ///< The existing target could not be renamed to ".bak.json", or the backup could not be removed.
  CommitFailed,                   ///< The ".tmp.json" file could not be renamed onto the target path, or the committed file failed re-validation.
  RecoveryFailed                  ///< recoverAtomicSave() could not reach a consistent state from the target/temp/backup files present.
};

/// @brief Outcome of a JsonStorage read/write/save operation.
struct JsonStorageResult {
  JsonStorageError error = JsonStorageError::Ok;  ///< Failure classification; JsonStorageError::Ok on success.
  std::size_t bytesProcessed = 0;  ///< Bytes read/written, meaning depends on the calling function.

  /// @brief Whether the operation succeeded.
  /// @return true if #error is JsonStorageError::Ok.
  constexpr bool ok() const { return error == JsonStorageError::Ok; }
};

/// @brief Stateless facade for validating, defaulting, and atomically
///        persisting every JSON document type StorageTask manages.
class JsonStorage {
 public:
  /// @brief Maximum accepted file size for a document type, used both to
  ///        reject oversized files on load and to size ArduinoJson buffers.
  /// @param documentType Document type to look up.
  /// @return Maximum size in bytes, or 0 if `documentType` is unrecognized.
  static std::size_t maxSizeFor(rtos::StorageDocumentType documentType);
  /// @brief Loads, parses, defaults, and validates a document from an open file.
  /// @param file Already-open file to read from.
  /// @param documentType Expected document type, used for size limits, defaulting, and validation.
  /// @param document Out parameter receiving the parsed document; cleared on any failure.
  /// @return Result with JsonStorageError::Ok on success, or the specific failure reason.
  static JsonStorageResult load(File& file,
                                rtos::StorageDocumentType documentType,
                                JsonDocument& document);
  /// @brief Fills in the type-agnostic "schemaVersion"/"updatedAt" fields if missing.
  /// @param document Document to update in place; must have an object root.
  /// @return JsonStorageError::Ok on success, or JsonStorageError::RootNotObject.
  static JsonStorageError applyDefaults(JsonDocument& document);
  /// @brief Validates the type-agnostic envelope fields ("schemaVersion"/"updatedAt").
  /// @param document Document to validate.
  /// @return JsonStorageError::Ok if valid, otherwise the specific violation.
  static JsonStorageError validate(const JsonDocument& document);
  /// @brief Builds a fresh, valid default document for a given type.
  /// @param documentType Document type to create.
  /// @param document Out parameter receiving the default document.
  /// @return JsonStorageError::Ok on success, or JsonStorageError::InvalidArgument for an unrecognized type.
  static JsonStorageError createDefault(
      rtos::StorageDocumentType documentType, JsonDocument& document);
  /// @brief Validates a document's envelope and its document-type-specific fields.
  /// @param document Document to validate.
  /// @param documentType Expected document type.
  /// @return JsonStorageError::Ok if valid, otherwise the specific violation.
  static JsonStorageError validate(
      const JsonDocument& document,
      rtos::StorageDocumentType documentType);
  /// @brief The "documentType" field value stamped into documents of a given type.
  /// @param documentType Document type to look up.
  /// @return Static type name string, or nullptr if `documentType` is unrecognized.
  static const char* documentTypeName(
      rtos::StorageDocumentType documentType);
  /// @brief Validates and serializes a document to a Print sink.
  /// @param document Document to serialize.
  /// @param documentType Expected document type, used for validation and size limits.
  /// @param output Sink to write the serialized JSON to.
  /// @return Result with the byte count written, or the validation/serialization failure reason.
  static JsonStorageResult serialize(
      const JsonDocument& document,
      rtos::StorageDocumentType documentType, Print& output);
  /// @brief Atomically writes a document to `targetPath`: serialize to a
  ///        ".tmp.json" file, validate it, back up any existing target to
  ///        ".bak.json", then commit by renaming the temp file onto the
  ///        target -- safe against power loss at any point in this sequence.
  /// @param filesystem Filesystem to operate on (the SD card).
  /// @param targetPath Final "/....json" path to write.
  /// @param documentType Expected document type, used for validation and size limits.
  /// @param document Document to save.
  /// @return Result with the byte count written, or the specific failure reason.
  static JsonStorageResult atomicSave(
      fs::FS& filesystem, const char* targetPath,
      rtos::StorageDocumentType documentType, const JsonDocument& document);
  /// @brief Restores a consistent state after an interrupted atomicSave(),
  ///        by checking target/".tmp.json"/".bak.json" in that preference
  ///        order and promoting the first one that validates.
  /// @param filesystem Filesystem to operate on (the SD card).
  /// @param targetPath Final "/....json" path that should end up valid.
  /// @param documentType Expected document type, used for validation.
  /// @return JsonStorageError::Ok if a consistent state was reached (including "no files exist at all"), otherwise JsonStorageError::RecoveryFailed.
  static JsonStorageResult recoverAtomicSave(
      fs::FS& filesystem, const char* targetPath,
      rtos::StorageDocumentType documentType);
  /// @brief Stable machine-readable name for a JsonStorageError, used in logs/diagnostics.
  /// @param error Error code to describe.
  /// @return Static, NUL-terminated snake_case name, or "unknown".
  static const char* errorName(JsonStorageError error);
};

}  // namespace filament_station::services
