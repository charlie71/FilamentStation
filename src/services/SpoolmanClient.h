/**
 * @file
 * @brief High-level Spoolman API operations for the NFC-tag-identity
 *        custom field (`extra.tag`): ensuring it exists, looking up a
 *        spool by tag, and setting/clearing it. Talks to Spoolman only
 *        through the injected SpoolmanHttpTransport, so it is unit
 *        testable without real HTTP.
 */
#pragma once

#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace services {

/// @brief Result of SpoolmanClient::ensureTagExtraField().
enum class TagExtraFieldStatus : std::uint8_t {
  Available,     ///< The `extra.tag` field already existed and is usable.
  Created,       ///< The field did not exist and was created successfully.
  Incompatible,  ///< A field with that name exists but has an incompatible type.
  Error,         ///< The request failed (network/server error).
};

/// @brief Result of SpoolmanClient::findSpoolByTag().
enum class TagLookupStatus : std::uint8_t {
  NotFound,   ///< No spool has this tag identity assigned.
  Found,      ///< Exactly one spool has this tag identity assigned.
  Duplicate,  ///< More than one spool has this tag identity assigned.
  Error,      ///< The request failed (network/server error).
};

/// @brief Outcome of a tag lookup, including the resolved spool if any.
struct TagLookupResult {
  TagLookupStatus status = TagLookupStatus::Error;  ///< Lookup outcome.
  std::uint32_t spoolId = 0;   ///< Resolved spool id, only valid if #status is Found.
  std::uint16_t matches = 0;   ///< Number of spools matched (>1 implies Duplicate).
  char error[96]{};            ///< Error detail, only valid if #status is Error.
};

/// @brief Outcome of a simple success/failure Spoolman operation.
struct SpoolmanOperationResult {
  bool success = false;  ///< Whether the operation succeeded.
  char error[96]{};       ///< Error detail, only valid if #success is false.
};

/// @brief HTTP transport abstraction injected into SpoolmanClient, so tests
///        can substitute a fake without performing real network I/O.
class SpoolmanHttpTransport {
 public:
  virtual ~SpoolmanHttpTransport() = default;
  /// @brief Performs a GET request.
  /// @param path API path, relative to the configured base URL.
  /// @param response Out parameter receiving the parsed JSON response.
  /// @param error Destination buffer for an error message on failure.
  /// @param errorCapacity Size of `error` in bytes.
  /// @return true on success.
  virtual bool get(const char* path, JsonDocument& response, char* error,
                   std::size_t errorCapacity) = 0;
  /// @brief Performs a POST request.
  /// @param path API path, relative to the configured base URL.
  /// @param request JSON request body.
  /// @param response Out parameter receiving the parsed JSON response.
  /// @param error Destination buffer for an error message on failure.
  /// @param errorCapacity Size of `error` in bytes.
  /// @return true on success.
  virtual bool post(const char* path, const JsonDocument& request,
                    JsonDocument& response, char* error,
                    std::size_t errorCapacity) = 0;
  /// @brief Performs a PATCH request.
  /// @param path API path, relative to the configured base URL.
  /// @param request JSON request body.
  /// @param response Out parameter receiving the parsed JSON response.
  /// @param error Destination buffer for an error message on failure.
  /// @param errorCapacity Size of `error` in bytes.
  /// @return true on success.
  virtual bool patch(const char* path, const JsonDocument& request,
                     JsonDocument& response, char* error,
                     std::size_t errorCapacity) = 0;
};

/// @brief Spoolman API client for tag-identity operations, built on top of
///        an injected SpoolmanHttpTransport.
class SpoolmanClient {
 public:
  /// @brief Constructs a client bound to the given transport.
  /// @param transport Transport used for every request; must outlive this client.
  explicit SpoolmanClient(SpoolmanHttpTransport& transport)
      : transport_(transport) {}

  /// @brief Ensures Spoolman's `extra.tag` custom field exists, creating it if necessary.
  /// @param error Destination buffer for an error message on failure.
  /// @param errorCapacity Size of `error` in bytes.
  /// @return The resulting field status.
  TagExtraFieldStatus ensureTagExtraField(char* error,
                                           std::size_t errorCapacity);
  /// @brief Looks up which spool (if any) has a given tag identity assigned.
  /// @param tagIdentity Canonical hex tag identity to search for.
  /// @return The lookup outcome.
  TagLookupResult findSpoolByTag(const char* tagIdentity);
  /// @brief Sets a spool's tag-identity field.
  /// @param spoolId Spool to update.
  /// @param tagIdentity Canonical hex tag identity to assign.
  /// @return Whether the update succeeded.
  SpoolmanOperationResult setSpoolTag(std::uint32_t spoolId,
                                      const char* tagIdentity);
  /// @brief Clears a spool's tag-identity field.
  /// @param spoolId Spool to update.
  /// @return Whether the update succeeded.
  SpoolmanOperationResult clearSpoolTag(std::uint32_t spoolId);

  /// @brief Decodes a JSON-encoded-string extra-field value into plain text.
  /// @param encoded Raw field value as stored by Spoolman (a JSON string literal).
  /// @param output Destination buffer receiving the decoded text.
  /// @param outputCapacity Size of `output` in bytes.
  /// @return false if the field is not a decodable string, or does not fit `output`.
  static bool decodeTextExtraField(JsonVariantConst encoded, char* output,
                                   std::size_t outputCapacity);

  /// @brief Decodes a JSON-encoded-string extra-field value into a number.
  /// @param encoded Raw field value as stored by Spoolman (a JSON string literal, even for numbers).
  /// @param output Out parameter receiving the decoded value.
  // Spoolman-Extra-Felder sind immer JSON-kodierte Strings, auch fuer
  // Zahlen (z. B. "bambu_temp_min": "\"220\"" oder "\"220.0\""). Liefert
  // false, wenn das Feld fehlt oder nicht als Zahl dekodierbar ist; ein
  // negativer oder Null-Wert wird bewusst NICHT hier abgelehnt (das
  // entscheidet der Aufrufer), nur das Dekodieren selbst wird gemeldet.
  /// @return false if the field is missing or not decodable as a number
  ///         (a negative or zero value is not rejected here; that is left to the caller).
  static bool decodeNumberExtraField(JsonVariantConst encoded, float& output);

 private:
  /// @brief Shared implementation for setSpoolTag()/clearSpoolTag().
  /// @param spoolId Spool to update.
  /// @param tagIdentity Tag identity to write, ignored when `clear` is true.
  /// @param clear Whether to clear the field instead of setting it.
  /// @return Whether the update succeeded.
  SpoolmanOperationResult updateSpoolTag(std::uint32_t spoolId,
                                         const char* tagIdentity,
                                         bool clear);
  SpoolmanHttpTransport& transport_;  ///< Injected HTTP transport used for every request.
};

}  // namespace services
}  // namespace filament_station
