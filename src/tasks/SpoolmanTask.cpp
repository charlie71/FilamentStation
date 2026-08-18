#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "models/SpoolmanSpool.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/SpoolmanCatalog.h"

namespace filament_station::tasks {
namespace {
models::SpoolmanSettings activeSettings{};

void sendResult(rtos::RtosContext& ctx, rtos::AppEventType type,
                std::uint32_t requestId, const char* text) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: result queue overflow");
}

bool getJson(const char* url, std::uint32_t timeoutMs, JsonDocument& document,
             char* error, std::size_t errorCapacity) {
  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(timeoutMs));
  http.setTimeout(static_cast<uint16_t>(timeoutMs));
  if (!http.begin(url)) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Server-URL");
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    std::snprintf(error, errorCapacity, "HTTP-Anfrage fehlgeschlagen (%d)",
                  status);
    http.end();
    return false;
  }
  const DeserializationError jsonError = deserializeJson(document, http.getStream());
  http.end();
  if (jsonError) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Serverantwort");
    return false;
  }
  return true;
}

bool postJson(const char* url, std::uint32_t timeoutMs,
              const JsonDocument& request, JsonDocument& response,
              char* error, std::size_t errorCapacity) {
  char payload[512]{};
  const std::size_t length = serializeJson(request, payload, sizeof(payload));
  if (length == 0 || length >= sizeof(payload)) {
    std::snprintf(error, errorCapacity, "Katalogdaten sind zu gross");
    return false;
  }
  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(timeoutMs));
  http.setTimeout(static_cast<uint16_t>(timeoutMs));
  if (!http.begin(url)) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Server-URL");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(
      reinterpret_cast<std::uint8_t*>(payload), length);
  if (status != HTTP_CODE_OK && status != HTTP_CODE_CREATED) {
    std::snprintf(error, errorCapacity,
                  "Spoolman hat Katalogdaten abgelehnt (%d)", status);
    http.end();
    return false;
  }
  const DeserializationError jsonError =
      deserializeJson(response, http.getStream());
  http.end();
  if (jsonError) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Serverantwort");
    return false;
  }
  return true;
}

bool patchJson(const char* url, std::uint32_t timeoutMs,
               const JsonDocument& request, JsonDocument& response,
               char* error, std::size_t errorCapacity) {
  char payload[384]{};
  const std::size_t length = serializeJson(request, payload, sizeof(payload));
  if (length == 0 || length >= sizeof(payload)) {
    std::snprintf(error, errorCapacity, "Gewichtsdaten sind zu gross");
    return false;
  }
  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(timeoutMs));
  http.setTimeout(static_cast<uint16_t>(timeoutMs));
  if (!http.begin(url)) {
    std::snprintf(error, errorCapacity, "Ungueltige Server-URL");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int status = http.PATCH(
      reinterpret_cast<std::uint8_t*>(payload), length);
  if (status != HTTP_CODE_OK) {
    const String responseBody = http.getString();
    JsonDocument errorDocument;
    const char* serverMessage = nullptr;
    if (!deserializeJson(errorDocument, responseBody))
      serverMessage = errorDocument["message"] | errorDocument["detail"] | nullptr;
    if (serverMessage != nullptr && serverMessage[0] != '\0')
      std::snprintf(error, errorCapacity, "Spoolman HTTP %d: %.88s", status,
                    serverMessage);
    else
      std::snprintf(error, errorCapacity, "Spoolman HTTP %d", status);
    char logMessage[224]{};
    std::snprintf(logMessage, sizeof(logMessage),
                  "SpoolmanTask: PATCH %s failed: %s", url, error);
    rtos::logLine(logMessage);
    http.end();
    return false;
  }
  const DeserializationError jsonError =
      deserializeJson(response, http.getStream());
  http.end();
  if (jsonError) {
    std::snprintf(error, errorCapacity, "Ungueltige Serverantwort");
    return false;
  }
  return true;
}

bool appendUrlEncoded(char* destination, std::size_t capacity,
                      const char* source) {
  constexpr char digits[] = "0123456789ABCDEF";
  std::size_t used = std::strlen(destination);
  for (const unsigned char* cursor =
           reinterpret_cast<const unsigned char*>(source);
       *cursor != '\0'; ++cursor) {
    const bool plain = std::isalnum(*cursor) || *cursor == '-' ||
                       *cursor == '_' || *cursor == '.' || *cursor == '~';
    const std::size_t required = plain ? 1U : 3U;
    if (used + required >= capacity) return false;
    if (plain) {
      destination[used++] = static_cast<char>(*cursor);
    } else {
      destination[used++] = '%';
      destination[used++] = digits[*cursor >> 4U];
      destination[used++] = digits[*cursor & 0x0FU];
    }
  }
  destination[used] = '\0';
  return true;
}

bool copyColorHex(char destination[9], const char* source,
                  std::size_t length = 0) {
  if (source == nullptr) return false;
  while (*source == '#' || *source == ' ') ++source;
  if (length == 0) length = std::strlen(source);
  while (length > 0 && source[length - 1] == ' ') --length;
  if (length != 6 && length != 8) return false;
  for (std::size_t index = 0; index < length; ++index) {
    const unsigned char value = static_cast<unsigned char>(source[index]);
    if (!std::isxdigit(value)) return false;
    destination[index] = static_cast<char>(std::toupper(value));
  }
  destination[length] = '\0';
  return true;
}

void parseSpoolColors(JsonObjectConst filament, models::SpoolmanSpool& spool) {
  const JsonVariantConst multi = filament["multi_color_hexes"];
  if (multi.is<JsonArrayConst>()) {
    for (JsonVariantConst value : multi.as<JsonArrayConst>()) {
      if (spool.colorCount >= models::SpoolmanSpool::kMaximumColors) break;
      const char* color = value.as<const char*>();
      if (copyColorHex(spool.colorHex[spool.colorCount], color))
        ++spool.colorCount;
    }
  } else if (multi.is<const char*>()) {
    const char* cursor = multi.as<const char*>();
    while (cursor != nullptr && *cursor != '\0' &&
           spool.colorCount < models::SpoolmanSpool::kMaximumColors) {
      const char* separator = std::strchr(cursor, ',');
      const std::size_t length = separator == nullptr
                                     ? std::strlen(cursor)
                                     : static_cast<std::size_t>(separator - cursor);
      if (copyColorHex(spool.colorHex[spool.colorCount], cursor, length))
        ++spool.colorCount;
      cursor = separator == nullptr ? nullptr : separator + 1;
    }
  }
  if (spool.colorCount == 0 &&
      copyColorHex(spool.colorHex[0], filament["color_hex"] | ""))
    spool.colorCount = 1;
}

bool parseSpool(JsonVariantConst source, models::SpoolmanSpool& spool) {
  if (!source["id"].is<std::uint32_t>() ||
      !source["filament"].is<JsonObjectConst>())
    return false;
  spool.id = source["id"].as<std::uint32_t>();
  const JsonObjectConst filament = source["filament"].as<JsonObjectConst>();
  std::snprintf(spool.filament, sizeof(spool.filament), "%s",
                filament["name"] | "-");
  std::snprintf(spool.material, sizeof(spool.material), "%s",
                filament["material"] | "-");
  parseSpoolColors(filament, spool);
  if (filament["vendor"].is<JsonObjectConst>())
    std::snprintf(spool.vendor, sizeof(spool.vendor), "%s",
                  filament["vendor"]["name"] | "-");
  else
    std::snprintf(spool.vendor, sizeof(spool.vendor), "-");
  spool.initialWeightGrams = source["initial_weight"] |
                             (filament["weight"] | 0.0F);
  spool.emptyWeightGrams = source["spool_weight"] |
                           (filament["spool_weight"] | 0.0F);
  spool.remainingWeightGrams = source["remaining_weight"] | 0.0F;
  spool.archived = source["archived"] | false;
  return spool.id != 0;
}

bool parseVendor(JsonVariantConst source, models::SpoolmanVendor& vendor) {
  if (!source["id"].is<std::uint32_t>() ||
      !source["name"].is<const char*>())
    return false;
  vendor.id = source["id"].as<std::uint32_t>();
  std::snprintf(vendor.name, sizeof(vendor.name), "%s",
                source["name"].as<const char*>());
  vendor.emptySpoolWeightGrams = source["empty_spool_weight"] | 0.0F;
  return vendor.id != 0;
}

bool parseFilament(JsonVariantConst source,
                   models::SpoolmanFilament& filament) {
  if (!source["id"].is<std::uint32_t>()) return false;
  filament.id = source["id"].as<std::uint32_t>();
  filament.vendorId = source["vendor"]["id"] |
                      (source["vendor_id"] | 0U);
  std::snprintf(filament.name, sizeof(filament.name), "%s",
                source["name"] | "");
  std::snprintf(filament.material, sizeof(filament.material), "%s",
                source["material"] | "");
  std::snprintf(filament.colorHex, sizeof(filament.colorHex), "%s",
                source["color_hex"] | "");
  filament.densityGramsPerCm3 = source["density"] | 0.0F;
  filament.diameterMillimeters = source["diameter"] | 0.0F;
  filament.weightGrams = source["weight"] | 0.0F;
  filament.emptySpoolWeightGrams = source["spool_weight"] | 0.0F;
  filament.nozzleTemperatureC = source["settings_extruder_temp"] | 0;
  filament.bedTemperatureC = source["settings_bed_temp"] | 0;
  return filament.id != 0;
}

bool catalogAvailable(rtos::RtosContext& ctx,
                      const models::SpoolmanSettings& settings,
                      std::uint32_t requestId) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) &
       rtos::EVENT_WIFI_CONNECTED) == 0) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, requestId,
               "Keine WLAN-Verbindung");
    return false;
  }
  if (!settings.enabled || settings.serverUrl[0] == '\0') {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, requestId,
               "Spoolman ist nicht konfiguriert");
    return false;
  }
  return true;
}

void sendCatalogItem(rtos::RtosContext& ctx, rtos::AppEventType type,
                     std::uint32_t requestId, std::int32_t index,
                     std::uint32_t id, const char* text) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  event.value = index;
  event.spoolId = id;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: catalog result queue overflow");
}

bool appendQuotedSearch(char* url, std::size_t capacity, const char* value) {
  char exact[68]{};
  const int written = std::snprintf(exact, sizeof(exact), "\"%s\"", value);
  return written > 0 && static_cast<std::size_t>(written) < sizeof(exact) &&
         appendUrlEncoded(url, capacity, exact);
}

void searchVendors(rtos::RtosContext& ctx,
                   const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;
  char url[256]{};
  std::snprintf(url, sizeof(url), "%s/vendor?limit=20&sort=name:asc",
                settings.serverUrl);
  if (command.searchText[0] != '\0') {
    std::strncat(url, "&name=", sizeof(url) - std::strlen(url) - 1);
    if (!appendUrlEncoded(url, sizeof(url), command.searchText)) {
      sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
                 "Herstellersuche ist zu lang");
      return;
    }
  }
  JsonDocument document;
  char error[96]{};
  if (!getJson(url, settings.timeoutMs, document, error, sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  std::int32_t count = 0;
  if (document.is<JsonArrayConst>()) {
    for (JsonVariantConst item : document.as<JsonArrayConst>()) {
      if (count >= 20) break;
      models::SpoolmanVendor vendor{};
      if (!parseVendor(item, vendor)) continue;
      sendCatalogItem(ctx, rtos::AppEventType::SpoolmanVendorResult,
                      command.requestId, count++, vendor.id, vendor.name);
    }
  }
  char message[64]{};
  std::snprintf(message, sizeof(message), "%ld Hersteller gefunden",
                static_cast<long>(count));
  sendResult(ctx, rtos::AppEventType::SpoolmanResponse, command.requestId,
             message);
}

void searchFilaments(rtos::RtosContext& ctx,
                     const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;
  char url[288]{};
  std::snprintf(url, sizeof(url), "%s/filament?limit=20&sort=name:asc",
                settings.serverUrl);
  if (command.filament.vendorId != 0) {
    const std::size_t used = std::strlen(url);
    std::snprintf(url + used, sizeof(url) - used, "&vendor.id=%lu",
                  static_cast<unsigned long>(command.filament.vendorId));
  }
  if (command.searchText[0] != '\0') {
    std::strncat(url, "&name=", sizeof(url) - std::strlen(url) - 1);
    if (!appendUrlEncoded(url, sizeof(url), command.searchText)) {
      sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
                 "Filamentsuche ist zu lang");
      return;
    }
  }
  JsonDocument document;
  char error[96]{};
  if (!getJson(url, settings.timeoutMs, document, error, sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  std::int32_t count = 0;
  if (document.is<JsonArrayConst>()) {
    for (JsonVariantConst item : document.as<JsonArrayConst>()) {
      if (count >= 20) break;
      models::SpoolmanFilament filament{};
      if (!parseFilament(item, filament)) continue;
      char text[160]{};
      std::snprintf(text, sizeof(text), "#%lu %s | %s | %s",
                    static_cast<unsigned long>(filament.id), filament.name,
                    filament.material,
                    filament.colorHex[0] != '\0' ? filament.colorHex : "-");
      sendCatalogItem(ctx, rtos::AppEventType::SpoolmanFilamentResult,
                      command.requestId, count++, filament.id, text);
    }
  }
  char message[64]{};
  std::snprintf(message, sizeof(message), "%ld Filamente gefunden",
                static_cast<long>(count));
  sendResult(ctx, rtos::AppEventType::SpoolmanResponse, command.requestId,
             message);
}

void createVendor(rtos::RtosContext& ctx,
                  const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;
  const auto validation = services::validateVendor(command.vendor);
  if (validation != services::CatalogValidationError::None) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               services::catalogValidationMessage(validation));
    return;
  }

  char url[288]{};
  std::snprintf(url, sizeof(url), "%s/vendor?limit=20&name=",
                settings.serverUrl);
  if (!appendQuotedSearch(url, sizeof(url), command.vendor.name)) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Herstellername ist zu lang");
    return;
  }
  JsonDocument matches;
  char error[96]{};
  if (!getJson(url, settings.timeoutMs, matches, error, sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  if (matches.is<JsonArrayConst>()) {
    for (JsonVariantConst item : matches.as<JsonArrayConst>()) {
      models::SpoolmanVendor existing{};
      if (parseVendor(item, existing) &&
          services::sameVendor(existing, command.vendor)) {
        sendCatalogItem(ctx, rtos::AppEventType::SpoolmanCatalogDuplicate,
                        command.requestId, 0, existing.id,
                        "Hersteller existiert bereits");
        return;
      }
    }
  }

  std::snprintf(url, sizeof(url), "%s/vendor", settings.serverUrl);
  JsonDocument request;
  request["name"] = command.vendor.name;
  request["empty_spool_weight"] = command.vendor.emptySpoolWeightGrams;
  JsonDocument response;
  if (!postJson(url, settings.timeoutMs, request, response, error,
                sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  models::SpoolmanVendor created{};
  if (!parseVendor(response.as<JsonVariantConst>(), created)) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Herstellerantwort ist unvollstaendig");
    return;
  }
  sendCatalogItem(ctx, rtos::AppEventType::SpoolmanCatalogCreated,
                  command.requestId, 0, created.id,
                  "Hersteller wurde angelegt");
}

void createFilament(rtos::RtosContext& ctx,
                    const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;
  const auto validation = services::validateFilament(command.filament);
  if (validation != services::CatalogValidationError::None) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               services::catalogValidationMessage(validation));
    return;
  }

  char url[320]{};
  std::snprintf(url, sizeof(url), "%s/filament?limit=20&vendor.id=%lu&name=",
                settings.serverUrl,
                static_cast<unsigned long>(command.filament.vendorId));
  if (!appendQuotedSearch(url, sizeof(url), command.filament.name)) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Filamentname ist zu lang");
    return;
  }
  JsonDocument matches;
  char error[96]{};
  if (!getJson(url, settings.timeoutMs, matches, error, sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  if (matches.is<JsonArrayConst>()) {
    for (JsonVariantConst item : matches.as<JsonArrayConst>()) {
      models::SpoolmanFilament existing{};
      if (parseFilament(item, existing) &&
          services::sameFilament(existing, command.filament)) {
        sendCatalogItem(ctx, rtos::AppEventType::SpoolmanCatalogDuplicate,
                        command.requestId, 0, existing.id,
                        "Filament existiert bereits");
        return;
      }
    }
  }

  std::snprintf(url, sizeof(url), "%s/filament", settings.serverUrl);
  JsonDocument request;
  request["vendor_id"] = command.filament.vendorId;
  request["name"] = command.filament.name;
  request["material"] = command.filament.material;
  request["density"] = command.filament.densityGramsPerCm3;
  request["diameter"] = command.filament.diameterMillimeters;
  if (command.filament.weightGrams > 0.0F)
    request["weight"] = command.filament.weightGrams;
  request["spool_weight"] = command.filament.emptySpoolWeightGrams;
  if (command.filament.colorHex[0] != '\0') {
    request["color_hex"] = command.filament.colorHex[0] == '#'
                               ? command.filament.colorHex + 1
                               : command.filament.colorHex;
  }
  if (command.filament.nozzleTemperatureC > 0)
    request["settings_extruder_temp"] =
        command.filament.nozzleTemperatureC;
  if (command.filament.bedTemperatureC > 0)
    request["settings_bed_temp"] = command.filament.bedTemperatureC;
  JsonDocument response;
  if (!postJson(url, settings.timeoutMs, request, response, error,
                sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  models::SpoolmanFilament created{};
  if (!parseFilament(response.as<JsonVariantConst>(), created)) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Filamentantwort ist unvollstaendig");
    return;
  }
  sendCatalogItem(ctx, rtos::AppEventType::SpoolmanCatalogCreated,
                  command.requestId, 0, created.id,
                  "Filament wurde angelegt");
}

bool findImportVendor(const models::SpoolmanSettings& settings,
                      const models::SpoolmanVendor& wanted,
                      models::SpoolmanVendor& result, char* error,
                      std::size_t errorCapacity) {
  char url[288]{};
  std::snprintf(url, sizeof(url), "%s/vendor?limit=20&name=",
                settings.serverUrl);
  if (!appendQuotedSearch(url, sizeof(url), wanted.name)) {
    std::snprintf(error, errorCapacity, "Herstellername ist zu lang");
    return false;
  }
  JsonDocument matches;
  if (!getJson(url, settings.timeoutMs, matches, error, errorCapacity))
    return false;
  if (!matches.is<JsonArrayConst>()) return true;
  for (JsonVariantConst item : matches.as<JsonArrayConst>()) {
    models::SpoolmanVendor candidate{};
    if (parseVendor(item, candidate) &&
        services::sameVendor(candidate, wanted)) {
      result = candidate;
      return true;
    }
  }
  return true;
}

bool createImportVendor(const models::SpoolmanSettings& settings,
                        const models::SpoolmanVendor& wanted,
                        models::SpoolmanVendor& result, char* error,
                        std::size_t errorCapacity) {
  char url[192]{};
  std::snprintf(url, sizeof(url), "%s/vendor", settings.serverUrl);
  JsonDocument request;
  request["name"] = wanted.name;
  if (wanted.emptySpoolWeightGrams > 0.0F)
    request["empty_spool_weight"] = wanted.emptySpoolWeightGrams;
  JsonDocument response;
  return postJson(url, settings.timeoutMs, request, response, error,
                  errorCapacity) &&
         (parseVendor(response.as<JsonVariantConst>(), result) ||
          (std::snprintf(error, errorCapacity,
                         "Herstellerantwort ist unvollstaendig"), false));
}

bool findImportFilament(const models::SpoolmanSettings& settings,
                        const models::SpoolmanFilament& wanted,
                        models::SpoolmanFilament& result, char* error,
                        std::size_t errorCapacity) {
  char url[320]{};
  std::snprintf(url, sizeof(url),
                "%s/filament?limit=20&vendor.id=%lu&name=",
                settings.serverUrl,
                static_cast<unsigned long>(wanted.vendorId));
  if (!appendQuotedSearch(url, sizeof(url), wanted.name)) {
    std::snprintf(error, errorCapacity, "Filamentname ist zu lang");
    return false;
  }
  JsonDocument matches;
  if (!getJson(url, settings.timeoutMs, matches, error, errorCapacity))
    return false;
  if (!matches.is<JsonArrayConst>()) return true;
  for (JsonVariantConst item : matches.as<JsonArrayConst>()) {
    models::SpoolmanFilament candidate{};
    if (parseFilament(item, candidate) &&
        services::sameFilament(candidate, wanted)) {
      result = candidate;
      return true;
    }
  }
  return true;
}

bool createImportFilament(const models::SpoolmanSettings& settings,
                          const models::SpoolmanFilament& wanted,
                          models::SpoolmanFilament& result, char* error,
                          std::size_t errorCapacity) {
  char url[192]{};
  std::snprintf(url, sizeof(url), "%s/filament", settings.serverUrl);
  JsonDocument request;
  request["vendor_id"] = wanted.vendorId;
  request["name"] = wanted.name;
  request["material"] = wanted.material;
  request["density"] = wanted.densityGramsPerCm3;
  request["diameter"] = wanted.diameterMillimeters;
  request["weight"] = wanted.weightGrams;
  if (wanted.emptySpoolWeightGrams > 0.0F)
    request["spool_weight"] = wanted.emptySpoolWeightGrams;
  if (wanted.colorHex[0] != '\0') request["color_hex"] = wanted.colorHex;
  if (wanted.nozzleTemperatureC > 0)
    request["settings_extruder_temp"] = wanted.nozzleTemperatureC;
  JsonDocument response;
  return postJson(url, settings.timeoutMs, request, response, error,
                  errorCapacity) &&
         (parseFilament(response.as<JsonVariantConst>(), result) ||
          (std::snprintf(error, errorCapacity,
                         "Filamentantwort ist unvollstaendig"), false));
}

void importTagDefinition(rtos::RtosContext& ctx,
                         const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;

  models::SpoolmanImportDefinition import{};
  const auto validation =
      services::mapTagDefinition(command.tagDefinition, import);
  if (validation != services::TagImportValidationError::None) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               services::tagImportValidationMessage(validation));
    return;
  }

  char error[128]{};
  models::SpoolmanVendor vendor{};
  if (!findImportVendor(settings, import.vendor, vendor, error,
                        sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  const bool reusedVendor = vendor.id != 0;
  if (!reusedVendor &&
      !createImportVendor(settings, import.vendor, vendor, error,
                          sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }

  import.filament.vendorId = vendor.id;
  models::SpoolmanFilament filament{};
  if (!findImportFilament(settings, import.filament, filament, error,
                          sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  const bool reusedFilament = filament.id != 0;
  if (!reusedFilament &&
      !createImportFilament(settings, import.filament, filament, error,
                            sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }

  char url[192]{};
  std::snprintf(url, sizeof(url), "%s/spool", settings.serverUrl);
  JsonDocument request;
  request["filament_id"] = filament.id;
  request["initial_weight"] = import.initialWeightGrams;
  if (import.emptySpoolWeightGrams > 0.0F)
    request["spool_weight"] = import.emptySpoolWeightGrams;
  JsonDocument response;
  if (!postJson(url, settings.timeoutMs, request, response, error,
                sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               error);
    return;
  }
  const std::uint32_t spoolId = response["id"] | 0U;
  if (spoolId == 0) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Spulenantwort enthaelt keine ID");
    return;
  }
  rtos::AppEvent completed{};
  completed.type = rtos::AppEventType::SpoolmanImportCompleted;
  completed.requestId = command.requestId;
  completed.spoolId = spoolId;
  completed.value = (reusedVendor ? 1 : 0) | (reusedFilament ? 2 : 0);
  std::snprintf(
      completed.text, sizeof(completed.text),
      "Spule #%lu angelegt. Hersteller: %s. Filament: %s.%s",
      static_cast<unsigned long>(spoolId),
      reusedVendor ? "vorhanden" : "neu",
      reusedFilament ? "vorhanden" : "neu",
      reusedVendor || reusedFilament
          ? " Vorhandene Katalogdaten wurden wiederverwendet."
          : "");
  if (xQueueSend(ctx.appEventQueue, &completed,
                 pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: import result queue overflow");
}

void sendSpool(rtos::RtosContext& ctx, std::uint32_t requestId,
               std::int32_t index, const models::SpoolmanSpool& spool) {
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::SpoolmanResponse;
  event.requestId = requestId;
  event.value = index;
  event.spoolId = spool.id;
  event.spool = spool;
  event.spoolColorCount = spool.colorCount;
  for (std::uint8_t color = 0; color < spool.colorCount; ++color)
    std::snprintf(event.spoolColorHex[color],
                  sizeof(event.spoolColorHex[color]), "%s",
                  spool.colorHex[color]);
  std::snprintf(event.text, sizeof(event.text),
                "#%lu  %.16s %.20s \xC2\xB7 %.10s \xC2\xB7 %.0f g%s",
                static_cast<unsigned long>(spool.id), spool.vendor,
                spool.filament, spool.material,
                static_cast<double>(spool.remainingWeightGrams),
                spool.archived ? " \xC2\xB7 archiviert" : "");
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: spool result queue overflow");
}

void loadSpools(rtos::RtosContext& ctx, const rtos::SpoolmanCommand& command) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_WIFI_CONNECTED) == 0) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Keine WLAN-Verbindung");
    return;
  }
  const models::SpoolmanSettings& settings =
      command.settings.serverUrl[0] != '\0' ? command.settings : activeSettings;
  if (!settings.enabled || settings.serverUrl[0] == '\0') {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Spoolman ist nicht konfiguriert");
    return;
  }

  char url[256]{};
  if (command.type == rtos::SpoolmanCommandType::LoadSpool) {
    std::snprintf(url, sizeof(url), "%s/spool/%lu", settings.serverUrl,
                  static_cast<unsigned long>(command.spoolId));
  } else {
    std::snprintf(url, sizeof(url), "%s/spool?allow_archived=%s&limit=20&sort=last_used:desc",
                  settings.serverUrl, command.includeArchived ? "true" : "false");
    if (command.searchText[0] != '\0') {
      const char* field = command.searchFilter == rtos::SpoolmanSearchFilter::Material
                              ? "filament.material"
                          : command.searchFilter == rtos::SpoolmanSearchFilter::Vendor
                              ? "filament.vendor.name"
                              : "filament.name";
      const std::size_t used = std::strlen(url);
      const int written = std::snprintf(url + used, sizeof(url) - used, "&%s=", field);
      if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(url) - used ||
          !appendUrlEncoded(url, sizeof(url), command.searchText)) {
        sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
                   "Suchbegriff ist zu lang");
        return;
      }
    }
  }

  JsonDocument document;
  char error[96]{};
  if (!getJson(url, settings.timeoutMs, document, error, sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }
  std::int32_t count = 0;
  if (command.type == rtos::SpoolmanCommandType::LoadSpool) {
    models::SpoolmanSpool spool{};
    if (parseSpool(document.as<JsonVariantConst>(), spool)) {
      sendSpool(ctx, command.requestId, count++, spool);
    }
  } else if (document.is<JsonArrayConst>()) {
    for (JsonVariantConst item : document.as<JsonArrayConst>()) {
      if (count >= 20) break;
      models::SpoolmanSpool spool{};
      if (parseSpool(item, spool)) sendSpool(ctx, command.requestId, count++, spool);
    }
  }
  rtos::AppEvent completed{};
  completed.type = rtos::AppEventType::SpoolmanResponse;
  completed.requestId = command.requestId;
  completed.value = -1;
  completed.spoolId = static_cast<rtos::SpoolId>(count);
  std::snprintf(completed.text, sizeof(completed.text), "%ld Spulen gefunden",
                static_cast<long>(count));
  if (xQueueSend(ctx.appEventQueue, &completed, pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: completion queue overflow");
}

void updateWeight(rtos::RtosContext& ctx,
                  const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  const auto& update = command.weightUpdate;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;
  if (services::validateWeightUpdate(update) !=
      services::WeightUpdateValidationError::None) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Ungueltige Gewichtsdaten");
    return;
  }
  char url[192]{};
  std::snprintf(url, sizeof(url), "%s/spool/%lu", settings.serverUrl,
                static_cast<unsigned long>(update.spoolId));
  JsonDocument request;
  request["remaining_weight"] = update.remainingWeightGrams;
  if (update.updateInitialWeight)
    request["initial_weight"] = update.initialWeightGrams;
  if (update.updateEmptySpoolWeight)
    request["spool_weight"] = update.emptySpoolWeightGrams;
  JsonDocument response;
  char error[128]{};
  if (!patchJson(url, settings.timeoutMs, request, response, error,
                 sizeof(error))) {
    rtos::AppEvent failed{};
    failed.type = rtos::AppEventType::SpoolmanError;
    failed.requestId = command.requestId;
    failed.spoolId = update.spoolId;
    failed.weightUpdate = update;
    std::snprintf(failed.text, sizeof(failed.text), "%s", error);
    if (xQueueSend(ctx.appEventQueue, &failed, pdMS_TO_TICKS(1000)) != pdPASS)
      rtos::logLine("SpoolmanTask: weight error queue overflow");
    return;
  }
  models::SpoolmanSpool spool{};
  if (!parseSpool(response.as<JsonVariantConst>(), spool)) {
    JsonDocument reloaded;
    if (!getJson(url, settings.timeoutMs, reloaded, error, sizeof(error)) ||
        !parseSpool(reloaded.as<JsonVariantConst>(), spool)) {
      sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
                 "Spule konnte nach dem Update nicht neu geladen werden");
      return;
    }
  }
  rtos::AppEvent completed{};
  completed.type = rtos::AppEventType::SpoolmanWeightUpdated;
  completed.requestId = command.requestId;
  completed.spoolId = spool.id;
  completed.weightUpdate = update;
  completed.weightUpdate.remainingWeightGrams = spool.remainingWeightGrams;
  completed.spoolColorCount = spool.colorCount;
  for (std::uint8_t index = 0; index < spool.colorCount; ++index)
    std::snprintf(completed.spoolColorHex[index],
                  sizeof(completed.spoolColorHex[index]), "%s",
                  spool.colorHex[index]);
  std::snprintf(completed.text, sizeof(completed.text),
                "%s|%s|%s|%.1f|%.1f", spool.vendor, spool.filament,
                spool.material, static_cast<double>(spool.emptyWeightGrams),
                static_cast<double>(spool.initialWeightGrams));
  if (xQueueSend(ctx.appEventQueue, &completed,
                 pdMS_TO_TICKS(1000)) != pdPASS)
    rtos::logLine("SpoolmanTask: weight result queue overflow");
}

void healthCheck(rtos::RtosContext& ctx, const rtos::SpoolmanCommand& command) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_WIFI_CONNECTED) == 0) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Keine WLAN-Verbindung");
    return;
  }
  char url[160]{};
  char error[96]{};
  JsonDocument health;
  std::snprintf(url, sizeof(url), "%s/health", command.settings.serverUrl);
  if (!getJson(url, command.settings.timeoutMs, health, error, sizeof(error))) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }

  JsonDocument info;
  std::snprintf(url, sizeof(url), "%s/info", command.settings.serverUrl);
  if (!getJson(url, command.settings.timeoutMs, info, error, sizeof(error))) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }
  const char* version = info["version"] | "unbekannt";
  char message[96]{};
  std::snprintf(message, sizeof(message), "Online | Version %s", version);
  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
  sendResult(ctx, rtos::AppEventType::SpoolmanConnected, command.requestId,
             message);
}
}  // namespace

void spoolmanTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::SpoolmanCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.spoolmanCommandQueue, &command, portMAX_DELAY) != pdPASS)
      continue;
    if (command.type == rtos::SpoolmanCommandType::ApplyConfiguration) {
      activeSettings = command.settings;
      if (!activeSettings.enabled)
        xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
    } else if (command.type == rtos::SpoolmanCommandType::HealthCheck) {
      healthCheck(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::LoadSpool ||
               command.type == rtos::SpoolmanCommandType::SearchSpools) {
      loadSpools(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::SearchVendors) {
      searchVendors(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::CreateVendor) {
      createVendor(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::SearchFilaments) {
      searchFilaments(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::CreateFilament) {
      createFilament(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::UpdateWeight) {
      updateWeight(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::ImportTagDefinition) {
      importTagDefinition(ctx, command);
    }
  }
}
}  // namespace filament_station::tasks
