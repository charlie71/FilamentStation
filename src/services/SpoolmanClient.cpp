#include "services/SpoolmanClient.h"

#include <cstdio>
#include <cstring>

namespace filament_station {
namespace services {
namespace {
constexpr char kFieldPath[] = "/field/spool";

bool tagFieldHasType(JsonVariantConst field, const char* type) {
  return field["key"].is<const char*>() &&
         std::strcmp(field["key"].as<const char*>(), "tag") == 0 &&
         field["field_type"].is<const char*>() &&
         std::strcmp(field["field_type"].as<const char*>(), type) == 0;
}

bool inspectTagField(JsonVariantConst response, bool& found,
                     bool& compatible) {
  found = false;
  compatible = false;
  if (!response.is<JsonArrayConst>()) return false;
  for (JsonVariantConst field : response.as<JsonArrayConst>()) {
    if (!field["key"].is<const char*>() ||
        std::strcmp(field["key"].as<const char*>(), "tag") != 0)
      continue;
    found = true;
    compatible = tagFieldHasType(field, "text");
    return true;
  }
  return true;
}

bool validIdentity(const char* value) {
  if (value == nullptr || value[0] == '\0') return false;
  const std::size_t length = std::strlen(value);
  if (length >= 40 || (length & 1U) != 0) return false;
  for (std::size_t index = 0; index < length; ++index) {
    const char c = value[index];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

void copyError(char* destination, std::size_t capacity, const char* source) {
  if (destination == nullptr || capacity == 0) return;
  std::snprintf(destination, capacity, "%s",
                source != nullptr && source[0] != '\0' ? source
                                                        : "Spoolman request failed");
}
}  // namespace

TagExtraFieldStatus SpoolmanClient::ensureTagExtraField(
    char* error, std::size_t errorCapacity) {
  if (error != nullptr && errorCapacity > 0) error[0] = '\0';
  JsonDocument fields;
  if (!transport_.get(kFieldPath, fields, error, errorCapacity))
    return TagExtraFieldStatus::Error;
  bool found = false;
  bool compatible = false;
  if (!inspectTagField(fields.as<JsonVariantConst>(), found, compatible)) {
    copyError(error, errorCapacity, "Invalid extra-field response");
    return TagExtraFieldStatus::Error;
  }
  if (found)
    return compatible ? TagExtraFieldStatus::Available
                      : TagExtraFieldStatus::Incompatible;

  JsonDocument request;
  request["name"] = "FilamentStation Tag";
  request["field_type"] = "text";
  JsonDocument response;
  if (!transport_.post("/field/spool/tag", request, response, error,
                       errorCapacity))
    return TagExtraFieldStatus::Error;
  if (!inspectTagField(response.as<JsonVariantConst>(), found, compatible) ||
      !found) {
    copyError(error, errorCapacity, "Created tag field was not returned");
    return TagExtraFieldStatus::Error;
  }
  return compatible ? TagExtraFieldStatus::Created
                    : TagExtraFieldStatus::Incompatible;
}

bool SpoolmanClient::decodeTextExtraField(JsonVariantConst encoded,
                                          char* output,
                                          std::size_t outputCapacity) {
  if (output == nullptr || outputCapacity == 0) return false;
  output[0] = '\0';
  if (!encoded.is<const char*>()) return false;
  JsonDocument decoded;
  if (deserializeJson(decoded, encoded.as<const char*>()) ||
      !decoded.is<const char*>())
    return false;
  const char* value = decoded.as<const char*>();
  if (std::strlen(value) >= outputCapacity) return false;
  std::snprintf(output, outputCapacity, "%s", value);
  return true;
}

TagLookupResult SpoolmanClient::findSpoolByTag(const char* tagIdentity) {
  TagLookupResult result{};
  if (!validIdentity(tagIdentity)) {
    copyError(result.error, sizeof(result.error), "Invalid tag identity");
    return result;
  }
  char path[160]{};
  // A double-quoted text filter is exact in Spoolman 0.26.x. Quotes are URL
  // encoded; the response is still compared after decoding as a safety check.
  std::snprintf(path, sizeof(path),
                "/spool?allow_archived=true&limit=3&extra.tag=%%22%s%%22",
                tagIdentity);
  JsonDocument response;
  if (!transport_.get(path, response, result.error, sizeof(result.error)))
    return result;
  if (!response.is<JsonArrayConst>()) {
    copyError(result.error, sizeof(result.error), "Invalid spool lookup response");
    return result;
  }
  for (JsonVariantConst spool : response.as<JsonArrayConst>()) {
    char decoded[40]{};
    if (!decodeTextExtraField(spool["extra"]["tag"], decoded,
                              sizeof(decoded)) ||
        std::strcmp(decoded, tagIdentity) != 0)
      continue;
    const std::uint32_t id = spool["id"] | 0U;
    if (id == 0) continue;
    if (result.matches == 0) result.spoolId = id;
    ++result.matches;
  }
  if (result.matches == 0)
    result.status = TagLookupStatus::NotFound;
  else if (result.matches == 1)
    result.status = TagLookupStatus::Found;
  else {
    result.status = TagLookupStatus::Duplicate;
    result.spoolId = 0;
  }
  return result;
}

SpoolmanOperationResult SpoolmanClient::setSpoolTag(
    std::uint32_t spoolId, const char* tagIdentity) {
  return updateSpoolTag(spoolId, tagIdentity, false);
}

SpoolmanOperationResult SpoolmanClient::clearSpoolTag(std::uint32_t spoolId) {
  return updateSpoolTag(spoolId, nullptr, true);
}

SpoolmanOperationResult SpoolmanClient::updateSpoolTag(
    std::uint32_t spoolId, const char* tagIdentity, bool clear) {
  SpoolmanOperationResult result{};
  if (spoolId == 0 || (!clear && !validIdentity(tagIdentity))) {
    copyError(result.error, sizeof(result.error), "Invalid spool tag update");
    return result;
  }
  char path[64]{};
  std::snprintf(path, sizeof(path), "/spool/%lu",
                static_cast<unsigned long>(spoolId));
  JsonDocument request;
  if (clear) {
    request["extra"]["tag"] = nullptr;
  } else {
    char encoded[48]{};
    JsonDocument value;
    value.set(tagIdentity);
    if (serializeJson(value, encoded, sizeof(encoded)) == 0) {
      copyError(result.error, sizeof(result.error), "Tag encoding failed");
      return result;
    }
    request["extra"]["tag"] = encoded;
  }
  JsonDocument response;
  if (!transport_.patch(path, request, response, result.error,
                        sizeof(result.error)))
    return result;
  if ((response["id"] | 0U) != spoolId ||
      !response["extra"].is<JsonObjectConst>()) {
    copyError(result.error, sizeof(result.error),
              "Invalid spool tag update response");
    return result;
  }
  if (clear) {
    result.success = response["extra"]["tag"].isNull();
  } else {
    char decoded[40]{};
    result.success = decodeTextExtraField(response["extra"]["tag"], decoded,
                                          sizeof(decoded)) &&
                     std::strcmp(decoded, tagIdentity) == 0;
  }
  if (!result.success)
    copyError(result.error, sizeof(result.error), "Spool tag verification failed");
  return result;
}

}  // namespace services
}  // namespace filament_station
