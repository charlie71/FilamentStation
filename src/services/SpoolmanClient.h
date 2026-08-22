#pragma once

#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace services {

enum class TagExtraFieldStatus : std::uint8_t {
  Available,
  Created,
  Incompatible,
  Error,
};

enum class TagLookupStatus : std::uint8_t {
  NotFound,
  Found,
  Duplicate,
  Error,
};

struct TagLookupResult {
  TagLookupStatus status = TagLookupStatus::Error;
  std::uint32_t spoolId = 0;
  std::uint16_t matches = 0;
  char error[96]{};
};

struct SpoolmanOperationResult {
  bool success = false;
  char error[96]{};
};

class SpoolmanHttpTransport {
 public:
  virtual ~SpoolmanHttpTransport() = default;
  virtual bool get(const char* path, JsonDocument& response, char* error,
                   std::size_t errorCapacity) = 0;
  virtual bool post(const char* path, const JsonDocument& request,
                    JsonDocument& response, char* error,
                    std::size_t errorCapacity) = 0;
  virtual bool patch(const char* path, const JsonDocument& request,
                     JsonDocument& response, char* error,
                     std::size_t errorCapacity) = 0;
};

class SpoolmanClient {
 public:
  explicit SpoolmanClient(SpoolmanHttpTransport& transport)
      : transport_(transport) {}

  TagExtraFieldStatus ensureTagExtraField(char* error,
                                           std::size_t errorCapacity);
  TagLookupResult findSpoolByTag(const char* tagIdentity);
  SpoolmanOperationResult setSpoolTag(std::uint32_t spoolId,
                                      const char* tagIdentity);
  SpoolmanOperationResult clearSpoolTag(std::uint32_t spoolId);

  static bool decodeTextExtraField(JsonVariantConst encoded, char* output,
                                   std::size_t outputCapacity);

  // Spoolman-Extra-Felder sind immer JSON-kodierte Strings, auch fuer
  // Zahlen (z. B. "bambu_temp_min": "\"220\"" oder "\"220.0\""). Liefert
  // false, wenn das Feld fehlt oder nicht als Zahl dekodierbar ist; ein
  // negativer oder Null-Wert wird bewusst NICHT hier abgelehnt (das
  // entscheidet der Aufrufer), nur das Dekodieren selbst wird gemeldet.
  static bool decodeNumberExtraField(JsonVariantConst encoded, float& output);

 private:
  SpoolmanOperationResult updateSpoolTag(std::uint32_t spoolId,
                                         const char* tagIdentity,
                                         bool clear);
  SpoolmanHttpTransport& transport_;
};

}  // namespace services
}  // namespace filament_station
