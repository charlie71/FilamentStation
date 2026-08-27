/**
 * @file
 * @brief Implements tasks::spoolmanTask(): the HTTP client for every
 *        Spoolman API operation (catalog search/create, spool load/search,
 *        weight updates, tag-identity field/lookup/assignment, and the
 *        connection health check).
 */
#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "models/SpoolmanSpool.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"
#include "services/PsramAlloc.h"
#include "services/SpoolmanCatalog.h"
#include "services/SpoolmanClient.h"

namespace filament_station::tasks {
namespace {
models::SpoolmanSettings activeSettings{};  ///< Settings from the most recent ApplyConfiguration, used as the fallback when a command carries no explicit settings.

/// @brief Sends a simple numeric/text AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param requestId Correlation id.
/// @param text Text payload.
/// @param value Numeric payload.
void sendResult(rtos::RtosContext& ctx, rtos::AppEventType type,
                std::uint32_t requestId, const char* text,
                std::int32_t value = 0) {
  // static, PSRAM-backed (services/PsramAlloc.h): AppEvent is a large
  // (~3KB), single-threaded, non-reentrant message struct (SpoolmanTask
  // processes exactly one command at a time). Kept off this task's stack
  // for the same reason NfcTask/ScaleTask/AppTask's AppEvent locals were
  // made static earlier this project -- healthCheck() (called from
  // ApplyConfiguration since the Spoolman auto-connect fix, i.e. on every
  // boot now, not just on a rare manual "Verbindung testen") triggered a
  // real stack-overflow reboot loop here. Moved off internal RAM entirely
  // (RAM-Optimierung 2026-08-25): 19 such buffers across the project added
  // up to tens of KB permanently reserved in internal DRAM for state that
  // is only ever bulk-copied through a queue.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.sendResult");
  *event = rtos::AppEvent{};
  event->type = type;
  event->requestId = requestId;
  event->value = value;
  std::snprintf(event->text, sizeof(event->text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=generic");
}

/// @brief Performs a GET request and parses the response body as JSON.
/// @param url Full request URL.
/// @param timeoutMs Connect/response timeout in milliseconds.
/// @param document Out parameter receiving the parsed response.
/// @param error Destination buffer for an error message on failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false on any request or parse failure.
bool getJson(const char* url, std::uint32_t timeoutMs, JsonDocument& document,
             char* error, std::size_t errorCapacity) {
  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(timeoutMs));
  http.setTimeout(static_cast<uint16_t>(timeoutMs));
  if (!http.begin(url)) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Server-URL");
    FS_LOGE(services::LogComponent::Spoolman,
            "Request failed method=GET url=\"%s\" error=\"%s\"", url, error);
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    std::snprintf(error, errorCapacity, "HTTP-Anfrage fehlgeschlagen (%d)",
                  status);
    FS_LOGE(services::LogComponent::Spoolman,
            "Request failed method=GET url=\"%s\" error=\"%s\"", url, error);
    http.end();
    return false;
  }
  const DeserializationError jsonError = deserializeJson(document, http.getStream());
  http.end();
  if (jsonError) {
    std::snprintf(error, errorCapacity, "Ung\xC3\xBCltige Serverantwort");
    FS_LOGE(services::LogComponent::Spoolman,
            "Request failed method=GET url=\"%s\" error=\"%s\" json_error=\"%s\"",
            url, error, jsonError.c_str());
    return false;
  }
  FS_LOGT(services::LogComponent::Spoolman,
          "Request succeeded method=GET url=\"%s\"", url);
  return true;
}

/// @brief Serializes a JSON body and performs a POST request, parsing the response as JSON.
/// @param url Full request URL.
/// @param timeoutMs Connect/response timeout in milliseconds.
/// @param request Request body to serialize and send.
/// @param response Out parameter receiving the parsed response.
/// @param error Destination buffer for an error message on failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false on any serialization, request, or parse failure.
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

/// @brief Serializes a JSON body and performs a PATCH request, parsing the
///        response as JSON and extracting a server-provided error message on failure.
/// @param url Full request URL.
/// @param timeoutMs Connect/response timeout in milliseconds.
/// @param request Request body to serialize and send.
/// @param response Out parameter receiving the parsed response.
/// @param error Destination buffer for an error message on failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false on any serialization, request, or parse failure.
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
    FS_LOGE(services::LogComponent::Spoolman,
            "Request failed method=PATCH url=\"%s\" error=\"%s\"", url,
            error);
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

/// @brief SpoolmanHttpTransport implementation backed by getJson()/postJson()/patchJson().
class TaskSpoolmanTransport final : public services::SpoolmanHttpTransport {
 public:
  /// @brief Constructs a transport bound to the given settings (base URL/timeout).
  /// @param settings Settings supplying the server URL and timeout; must outlive this transport.
  explicit TaskSpoolmanTransport(const models::SpoolmanSettings& settings)
      : settings_(settings) {}

  bool get(const char* path, JsonDocument& response, char* error,
           std::size_t errorCapacity) override {
    char url[320]{};
    if (!makeUrl(path, url, sizeof(url), error, errorCapacity)) return false;
    return getJson(url, settings_.timeoutMs, response, error, errorCapacity);
  }

  bool post(const char* path, const JsonDocument& request,
            JsonDocument& response, char* error,
            std::size_t errorCapacity) override {
    char url[320]{};
    if (!makeUrl(path, url, sizeof(url), error, errorCapacity)) return false;
    return postJson(url, settings_.timeoutMs, request, response, error,
                    errorCapacity);
  }

  bool patch(const char* path, const JsonDocument& request,
             JsonDocument& response, char* error,
             std::size_t errorCapacity) override {
    char url[320]{};
    if (!makeUrl(path, url, sizeof(url), error, errorCapacity)) return false;
    return patchJson(url, settings_.timeoutMs, request, response, error,
                     errorCapacity);
  }

 private:
  /// @brief Concatenates the configured server URL with an API path.
  /// @param path API path to append.
  /// @param url Destination buffer receiving the full URL.
  /// @param capacity Size of `url` in bytes.
  /// @param error Destination buffer for an error message on failure.
  /// @param errorCapacity Size of `error` in bytes.
  /// @return false if the result would not fit `url`.
  bool makeUrl(const char* path, char* url, std::size_t capacity, char* error,
               std::size_t errorCapacity) const {
    const int written = std::snprintf(url, capacity, "%s%s",
                                      settings_.serverUrl, path);
    if (written > 0 && static_cast<std::size_t>(written) < capacity) return true;
    std::snprintf(error, errorCapacity, "Spoolman URL is too long");
    return false;
  }
  const models::SpoolmanSettings& settings_;  ///< Settings supplying the server URL and timeout.
};

/// @brief Percent-encodes and appends `source` to `destination`.
/// @param destination Buffer to append to; must already be NUL-terminated.
/// @param capacity Size of `destination` in bytes.
/// @param source Text to encode and append.
/// @return false if the result would not fit `destination`.
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

/// @brief Normalizes a color-hex string (strips '#'/spaces, uppercases, validates length) into a fixed buffer.
/// @param destination Destination buffer, 9 bytes.
/// @param source Source color string.
/// @param length Length of `source` to consider, or 0 to use its full NUL-terminated length.
/// @return false if `source` is null or not a valid 6/8-digit hex string.
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

/// @brief Parses a spool's color(s) from its filament's multi_color_hexes/color_hex fields.
/// @param filament Filament JSON object from a spool response.
/// @param spool Out parameter receiving the parsed colors in `colorHex`/`colorCount`.
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

/// @brief Parses a Spoolman "/spool" response into a models::SpoolmanSpool.
/// @param source Parsed JSON spool object.
/// @param spool Out parameter receiving the decoded fields.
/// @return false if required fields (id, filament object) are missing/invalid.
bool parseSpool(JsonVariantConst source, models::SpoolmanSpool& spool) {
  if (!source["id"].is<std::uint32_t>() ||
      !source["filament"].is<JsonObjectConst>())
    return false;
  spool.id = source["id"].as<std::uint32_t>();
  const JsonObjectConst filament = source["filament"].as<JsonObjectConst>();
  spool.filamentId = filament["id"] | 0U;
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
  const JsonVariantConst extraTag = source["extra"]["tag"];
  spool.extraTagPresent = !extraTag.isNull();
  spool.extraTagValid =
      !spool.extraTagPresent || services::SpoolmanClient::decodeTextExtraField(
                                    extraTag, spool.extraTag,
                                    sizeof(spool.extraTag));
  // bambu_temp_min/bambu_temp_max/flow_dynamics_k_factor are deliberately NOT parsed
  // here anymore: they are Spoolman *filament* properties (Nutzerhinweis
  // 2026-08-24), fetched via a dedicated SpoolmanCommandType::LoadFilament
  // request (see loadFilament()/parseFilament() below) using spool.filamentId,
  // rather than trusted from this spool response's embedded filament object.
  return spool.id != 0;
}

/// @brief Parses a Spoolman "/vendor" response into a models::SpoolmanVendor.
/// @param source Parsed JSON vendor object.
/// @param vendor Out parameter receiving the decoded fields.
/// @return false if required fields (id, name) are missing/invalid.
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

/// @brief Parses a Spoolman "/filament" response into a models::SpoolmanFilament,
///        including the project-specific Bambu temperature/K-factor extra fields.
/// @param source Parsed JSON filament object.
/// @param filament Out parameter receiving the decoded fields.
/// @return false if the required "id" field is missing/invalid.
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
  // Bambu-Duesentemperatur: projektspezifische Extra-Felder (kein
  // Spoolman-Standardfeld, siehe docs/bambu-protocol.md). Beide Felder
  // muessen vorhanden und als Zahl > 0 dekodierbar sein, sonst bleibt
  // bambuTempFieldsValid false und der Aufrufer zeigt einen Hinweis statt
  // eine erfundene Temperatur zu senden. "extra" liegt hier auf Root-Ebene
  // (direkter Filament-Fetch), nicht unter einem verschachtelten
  // "filament"-Schluessel wie in einer Spool-Antwort.
  const JsonVariantConst extraTempMin = source["extra"]["bambu_temp_min"];
  const JsonVariantConst extraTempMax = source["extra"]["bambu_temp_max"];
  filament.bambuTempFieldsPresent =
      !extraTempMin.isNull() && !extraTempMax.isNull();
  float tempMin = 0.0F;
  float tempMax = 0.0F;
  filament.bambuTempFieldsValid =
      filament.bambuTempFieldsPresent &&
      services::SpoolmanClient::decodeNumberExtraField(extraTempMin, tempMin) &&
      services::SpoolmanClient::decodeNumberExtraField(extraTempMax, tempMax) &&
      tempMin > 0.0F && tempMax > 0.0F && tempMin <= tempMax;
  if (filament.bambuTempFieldsValid) {
    filament.bambuTempMinC = static_cast<std::uint16_t>(tempMin);
    filament.bambuTempMaxC = static_cast<std::uint16_t>(tempMax);
  }
  // Bambu-K-Faktor: Anzeige-only (Nutzerwunsch 2026-08-24), kein Einfluss
  // auf das an den Drucker gesendete Kommando -- daher genuegt "> 0", keine
  // Plausibilitaetsspanne wie bei den Duesentemperaturen noetig.
  // Feldname vom Nutzer bestaetigt (2026-08-24): "flow_dynamics_k_factor",
  // nicht "bambu_k_factor".
  const JsonVariantConst extraKFactor =
      source["extra"]["flow_dynamics_k_factor"];
  filament.bambuKFactorPresent = !extraKFactor.isNull();
  float kFactor = 0.0F;
  filament.bambuKFactorValid =
      filament.bambuKFactorPresent &&
      services::SpoolmanClient::decodeNumberExtraField(extraKFactor, kFactor) &&
      kFactor > 0.0F;
  if (filament.bambuKFactorValid) {
    filament.bambuKFactor = kFactor;
  }
  return filament.id != 0;
}

/// @brief Checks WiFi connectivity and Spoolman configuration, sending a SpoolmanError if unavailable.
/// @param ctx Owning RTOS context.
/// @param settings Settings to check.
/// @param requestId Correlation id for the error event.
/// @return true if a request can proceed.
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

/// @brief Sends one search-result/catalog-action AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param requestId Correlation id.
/// @param index Result index (or 0 for a single-item action).
/// @param id Vendor/filament/spool id.
/// @param text Text payload (display line, or status message).
void sendCatalogItem(rtos::RtosContext& ctx, rtos::AppEventType type,
                     std::uint32_t requestId, std::int32_t index,
                     std::uint32_t id, const char* text) {
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.sendCatalogItem");
  *event = rtos::AppEvent{};
  event->type = type;
  event->requestId = requestId;
  event->value = index;
  event->spoolId = id;
  std::snprintf(event->text, sizeof(event->text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=catalog");
}

/// @brief Percent-encodes and appends `"value"` (double-quoted, for an exact-match filter) to a URL.
/// @param url Buffer to append to; must already be NUL-terminated.
/// @param capacity Size of `url` in bytes.
/// @param value Text to quote, encode, and append.
/// @return false if the result would not fit.
bool appendQuotedSearch(char* url, std::size_t capacity, const char* value) {
  char exact[68]{};
  const int written = std::snprintf(exact, sizeof(exact), "\"%s\"", value);
  return written > 0 && static_cast<std::size_t>(written) < sizeof(exact) &&
         appendUrlEncoded(url, capacity, exact);
}

/// @brief Handles SpoolmanCommandType::SearchVendors: queries and reports matching vendors.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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

/// @brief Handles SpoolmanCommandType::SearchFilaments: queries and reports matching filaments.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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

/// @brief Handles SpoolmanCommandType::CreateVendor: finds-or-creates a vendor, reporting duplicate/created.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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

/// @brief Handles SpoolmanCommandType::CreateFilament: finds-or-creates a filament, reporting duplicate/created.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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

/// @brief Searches for a matching existing vendor during tag import.
/// @param settings Settings supplying the server URL/timeout.
/// @param wanted Vendor to search for.
/// @param result Out parameter receiving the matching vendor, if found (result.id stays 0 if not found).
/// @param error Destination buffer for an error message on request failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false only on a request failure (not-found is not a failure).
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

/// @brief Creates a new vendor during tag import.
/// @param settings Settings supplying the server URL/timeout.
/// @param wanted Vendor fields to create.
/// @param result Out parameter receiving the created vendor.
/// @param error Destination buffer for an error message on failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false on request failure or an incomplete response.
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

/// @brief Searches for a matching existing filament during tag import.
/// @param settings Settings supplying the server URL/timeout.
/// @param wanted Filament to search for (its vendorId must already be resolved).
/// @param result Out parameter receiving the matching filament, if found (result.id stays 0 if not found).
/// @param error Destination buffer for an error message on request failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false only on a request failure (not-found is not a failure).
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

/// @brief Creates a new filament during tag import.
/// @param settings Settings supplying the server URL/timeout.
/// @param wanted Filament fields to create.
/// @param result Out parameter receiving the created filament.
/// @param error Destination buffer for an error message on failure.
/// @param errorCapacity Size of `error` in bytes.
/// @return false on request failure or an incomplete response.
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

/// @brief Handles SpoolmanCommandType::ImportTagDefinition: maps the tag
///        data, finds-or-creates the vendor/filament, and creates a new
///        spool from it.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* completed =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.importCompleted");
  *completed = rtos::AppEvent{};
  completed->type = rtos::AppEventType::SpoolmanImportCompleted;
  completed->requestId = command.requestId;
  completed->spoolId = spoolId;
  completed->value = (reusedVendor ? 1 : 0) | (reusedFilament ? 2 : 0);
  std::snprintf(
      completed->text, sizeof(completed->text),
      "Spule #%lu angelegt. Hersteller: %s. Filament: %s.%s",
      static_cast<unsigned long>(spoolId),
      reusedVendor ? "vorhanden" : "neu",
      reusedFilament ? "vorhanden" : "neu",
      reusedVendor || reusedFilament
          ? " Vorhandene Katalogdaten wurden wiederverwendet."
          : "");
  if (xQueueSend(ctx.appEventQueue, completed,
                 pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=import");
}

/// @brief Sends one spool as a SpoolmanResponse AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param index Result index within a search, or 0 for a single-spool load.
/// @param spool Spool to send.
void sendSpool(rtos::RtosContext& ctx, std::uint32_t requestId,
               std::int32_t index, const models::SpoolmanSpool& spool) {
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.sendSpool");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::SpoolmanResponse;
  event->requestId = requestId;
  event->value = index;
  event->spoolId = spool.id;
  event->spool = spool;
  event->spoolColorCount = spool.colorCount;
  for (std::uint8_t color = 0; color < spool.colorCount; ++color)
    std::snprintf(event->spoolColorHex[color],
                  sizeof(event->spoolColorHex[color]), "%s",
                  spool.colorHex[color]);
  std::snprintf(event->text, sizeof(event->text),
                "#%lu  %.16s %.20s \xC2\xB7 %.10s \xC2\xB7 %.0f g%s",
                static_cast<unsigned long>(spool.id), spool.vendor,
                spool.filament, spool.material,
                static_cast<double>(spool.remainingWeightGrams),
                spool.archived ? " \xC2\xB7 archiviert" : "");
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=spool");
}

/// @brief Sends a filament as a SpoolmanResponse AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param filament Filament to send.
void sendFilamentDetails(rtos::RtosContext& ctx, std::uint32_t requestId,
                         const models::SpoolmanFilament& filament) {
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.sendFilamentDetails");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::SpoolmanResponse;
  event->requestId = requestId;
  event->value = 0;
  event->filament = filament;
  std::snprintf(event->text, sizeof(event->text), "#%lu  %.32s \xC2\xB7 %.20s",
               static_cast<unsigned long>(filament.id), filament.name,
               filament.material);
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=filament_details");
}

/// @brief Handles SpoolmanCommandType::LoadFilament: GET /filament/{id} directly.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
// GET /filament/{id} directly, instead of trusting the (possibly
// incomplete) nested filament object embedded in a spool response --
// bambu_temp_min/bambu_temp_max/flow_dynamics_k_factor are filament
// properties, see docs/bambu-protocol.md.
void loadFilamentDetails(rtos::RtosContext& ctx,
                         const rtos::SpoolmanCommand& command) {
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
  if (command.filamentId == 0) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Ung\xC3\xBCltige Filament-ID");
    return;
  }
  char url[256]{};
  std::snprintf(url, sizeof(url), "%s/filament/%lu", settings.serverUrl,
               static_cast<unsigned long>(command.filamentId));
  JsonDocument document;
  char error[96]{};
  if (!getJson(url, settings.timeoutMs, document, error, sizeof(error))) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }
  models::SpoolmanFilament filament{};
  if (!parseFilament(document.as<JsonVariantConst>(), filament)) {
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Filamentdaten sind ung\xC3\xBCltig");
    FS_LOGE(services::LogComponent::Spoolman,
            "Filament parse failed filament_id=%lu",
            static_cast<unsigned long>(command.filamentId));
    return;
  }
  FS_LOGD(services::LogComponent::Spoolman,
          "Filament loaded request_id=%lu filament_id=%lu "
          "temp_fields_present=%d temp_fields_valid=%d temp_min=%u "
          "temp_max=%u kfactor_present=%d kfactor_valid=%d kfactor=%.3f",
          static_cast<unsigned long>(command.requestId),
          static_cast<unsigned long>(filament.id),
          filament.bambuTempFieldsPresent, filament.bambuTempFieldsValid,
          filament.bambuTempMinC, filament.bambuTempMaxC,
          filament.bambuKFactorPresent, filament.bambuKFactorValid,
          static_cast<double>(filament.bambuKFactor));
  sendFilamentDetails(ctx, command.requestId, filament);
}

/// @brief Handles SpoolmanCommandType::LoadSpool/SearchSpools: loads a
///        single spool by id, or searches with the given filter/text.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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

  FS_LOGD(services::LogComponent::Spoolman,
          "Querying spool(s) request_id=%lu url=\"%s\"",
          static_cast<unsigned long>(command.requestId), url);
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
      FS_LOGD(services::LogComponent::Spoolman,
              "Spool loaded request_id=%lu spool_id=%lu filament_id=%lu "
              "vendor=\"%s\" material=\"%s\" empty_weight=%.1f "
              "initial_weight=%.1f remaining_weight=%.1f",
              static_cast<unsigned long>(command.requestId),
              static_cast<unsigned long>(spool.id),
              static_cast<unsigned long>(spool.filamentId), spool.vendor,
              spool.material, static_cast<double>(spool.emptyWeightGrams),
              static_cast<double>(spool.initialWeightGrams),
              static_cast<double>(spool.remainingWeightGrams));
      sendSpool(ctx, command.requestId, count++, spool);
    } else {
      FS_LOGE(services::LogComponent::Spoolman,
              "Spool parse failed request_id=%lu spool_id=%lu",
              static_cast<unsigned long>(command.requestId),
              static_cast<unsigned long>(command.spoolId));
    }
  } else if (document.is<JsonArrayConst>()) {
    for (JsonVariantConst item : document.as<JsonArrayConst>()) {
      if (count >= 20) break;
      models::SpoolmanSpool spool{};
      if (parseSpool(item, spool)) sendSpool(ctx, command.requestId, count++, spool);
    }
    FS_LOGD(services::LogComponent::Spoolman,
            "Spool search request_id=%lu url=\"%s\" results=%ld",
            static_cast<unsigned long>(command.requestId), url,
            static_cast<long>(count));
  }
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* completed =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.loadSpools.completed");
  *completed = rtos::AppEvent{};
  completed->type = rtos::AppEventType::SpoolmanResponse;
  completed->requestId = command.requestId;
  completed->value = -1;
  completed->spoolId = static_cast<rtos::SpoolId>(count);
  std::snprintf(completed->text, sizeof(completed->text), "%ld Spulen gefunden",
                static_cast<long>(count));
  if (xQueueSend(ctx.appEventQueue, completed, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=completion");
}

/// @brief Handles SpoolmanCommandType::UpdateWeight: PATCHes remaining/initial/empty-spool weight.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
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
    // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
    static rtos::AppEvent* failed =
        services::allocatePsramInstance<rtos::AppEvent>(
            "SpoolmanTask.updateWeight.failed");
    *failed = rtos::AppEvent{};
    failed->type = rtos::AppEventType::SpoolmanError;
    failed->requestId = command.requestId;
    failed->spoolId = update.spoolId;
    failed->weightUpdate = update;
    std::snprintf(failed->text, sizeof(failed->text), "%s", error);
    if (xQueueSend(ctx.appEventQueue, failed, pdMS_TO_TICKS(1000)) != pdPASS)
      FS_LOGW(services::LogComponent::Spoolman,
              "Event enqueue failed queue=app_event result=weight_error");
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
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* completed =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.updateWeight.completed");
  *completed = rtos::AppEvent{};
  completed->type = rtos::AppEventType::SpoolmanWeightUpdated;
  completed->requestId = command.requestId;
  completed->spoolId = spool.id;
  completed->weightUpdate = update;
  completed->weightUpdate.remainingWeightGrams = spool.remainingWeightGrams;
  completed->spoolColorCount = spool.colorCount;
  for (std::uint8_t index = 0; index < spool.colorCount; ++index)
    std::snprintf(completed->spoolColorHex[index],
                  sizeof(completed->spoolColorHex[index]), "%s",
                  spool.colorHex[index]);
  std::snprintf(completed->text, sizeof(completed->text),
                "%s|%s|%s|%.1f|%.1f", spool.vendor, spool.filament,
                spool.material, static_cast<double>(spool.emptyWeightGrams),
                static_cast<double>(spool.initialWeightGrams));
  if (xQueueSend(ctx.appEventQueue, completed,
                 pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=weight");
}

/// @brief Handles the tag-identity commands (EnsureTagExtraField/FindSpoolByTag/SetSpoolTag/ClearSpoolTag)
///        via services::SpoolmanClient.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
void executeTagClientCommand(rtos::RtosContext& ctx,
                             const rtos::SpoolmanCommand& command) {
  const auto& settings = command.settings.serverUrl[0] != '\0'
                             ? command.settings
                             : activeSettings;
  if (!catalogAvailable(ctx, settings, command.requestId)) return;
  TaskSpoolmanTransport transport(settings);
  services::SpoolmanClient client(transport);
  // static, PSRAM-backed: see sendResult() above / services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "SpoolmanTask.executeTagClientCommand");
  *event = rtos::AppEvent{};
  event->requestId = command.requestId;
  event->tagIdentity = command.tagIdentity;

  if (command.type == rtos::SpoolmanCommandType::EnsureTagExtraField) {
    char error[96]{};
    const auto status = client.ensureTagExtraField(error, sizeof(error));
    event->value = static_cast<std::int32_t>(status);
    if (status == services::TagExtraFieldStatus::Available ||
        status == services::TagExtraFieldStatus::Created) {
      event->type = rtos::AppEventType::SpoolmanTagFieldReady;
      xEventGroupSetBits(ctx.systemEventGroup,
                         rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
      std::snprintf(event->text, sizeof(event->text), "%s",
                    status == services::TagExtraFieldStatus::Created
                        ? "Spoolman tag field created"
                        : "Spoolman tag field ready");
    } else {
      event->type = rtos::AppEventType::SpoolmanError;
      xEventGroupClearBits(ctx.systemEventGroup,
                           rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
      std::snprintf(event->text, sizeof(event->text), "%s",
                    status == services::TagExtraFieldStatus::Incompatible
                        ? "Spoolman extra field 'tag' must have type text"
                        : error);
    }
  } else if (command.type == rtos::SpoolmanCommandType::FindSpoolByTag) {
    const auto result = client.findSpoolByTag(command.tagIdentity.value);
    event->spoolId = result.spoolId;
    if (result.status == services::TagLookupStatus::Error) {
      event->value = static_cast<std::int32_t>(result.status);
      event->type = rtos::AppEventType::SpoolmanError;
      std::snprintf(event->text, sizeof(event->text), "%s", result.error);
    } else if (result.status == services::TagLookupStatus::Duplicate) {
      event->type = rtos::AppEventType::SpoolmanTagDuplicate;
      event->value = static_cast<std::int32_t>(result.matches);
      std::snprintf(event->text, sizeof(event->text),
                    "Duplicate tag assignment: %u matching spools",
                    static_cast<unsigned>(result.matches));
      FS_LOGE(services::LogComponent::Spoolman,
              "Duplicate tag assignment tag=%s matches=%u",
              command.tagIdentity.value,
              static_cast<unsigned>(result.matches));
    } else {
      event->value = static_cast<std::int32_t>(result.status);
      event->type = rtos::AppEventType::SpoolmanTagLookup;
      std::snprintf(event->text, sizeof(event->text), "matches=%u",
                    static_cast<unsigned>(result.matches));
    }
  } else {
    const auto result = command.type == rtos::SpoolmanCommandType::SetSpoolTag
                            ? client.setSpoolTag(command.spoolId,
                                                 command.tagIdentity.value)
                            : client.clearSpoolTag(command.spoolId);
    event->spoolId = command.spoolId;
    event->type = result.success ? rtos::AppEventType::SpoolmanTagUpdated
                                : rtos::AppEventType::SpoolmanError;
    std::snprintf(event->text, sizeof(event->text), "%s",
                  result.success ? "Spoolman tag assignment updated"
                                 : result.error);
  }
  if (xQueueSend(ctx.appEventQueue, event, pdMS_TO_TICKS(1000)) != pdPASS)
    FS_LOGW(services::LogComponent::Spoolman,
            "Event enqueue failed queue=app_event result=tag_client");
}

/// @brief Handles SpoolmanCommandType::ApplyConfiguration/HealthCheck:
///        checks /health and /info, ensures the tag extra field, and
///        updates EVENT_SPOOLMAN_READY/EVENT_SPOOLMAN_TAG_FIELD_READY.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
void healthCheck(rtos::RtosContext& ctx, const rtos::SpoolmanCommand& command) {
  if ((xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_WIFI_CONNECTED) == 0) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY |
                                                   rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
               "Keine WLAN-Verbindung");
    return;
  }
  char url[160]{};
  char error[96]{};
  JsonDocument health;
  std::snprintf(url, sizeof(url), "%s/health", command.settings.serverUrl);
  if (!getJson(url, command.settings.timeoutMs, health, error, sizeof(error))) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY |
                                                   rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }

  JsonDocument info;
  std::snprintf(url, sizeof(url), "%s/info", command.settings.serverUrl);
  if (!getJson(url, command.settings.timeoutMs, info, error, sizeof(error))) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY |
                                                   rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
    sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId, error);
    return;
  }
  const char* version = info["version"] | "unbekannt";
  TaskSpoolmanTransport transport(command.settings);
  services::SpoolmanClient client(transport);
  char fieldError[96]{};
  const auto fieldStatus =
      client.ensureTagExtraField(fieldError, sizeof(fieldError));
  const bool tagFieldReady =
      fieldStatus == services::TagExtraFieldStatus::Available ||
      fieldStatus == services::TagExtraFieldStatus::Created;
  if (tagFieldReady) {
    xEventGroupSetBits(ctx.systemEventGroup,
                       rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
    FS_LOGI(services::LogComponent::Spoolman,
            "Tag extra field ready status=%s",
            fieldStatus == services::TagExtraFieldStatus::Created ? "created"
                                                                  : "available");
  } else {
    xEventGroupClearBits(ctx.systemEventGroup,
                         rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
    FS_LOGE(services::LogComponent::Spoolman,
            "Tag extra field unavailable reason=\"%s\"",
            fieldStatus == services::TagExtraFieldStatus::Incompatible
                ? "field type is not text"
                : fieldError);
  }
  char message[96]{};
  std::snprintf(message, sizeof(message), "Online | Version %s | Tag-Feld %s",
                version, tagFieldReady ? "bereit" : "nicht verfuegbar");
  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY);
  sendResult(ctx, rtos::AppEventType::SpoolmanConnected, command.requestId,
             message, static_cast<std::int32_t>(fieldStatus));
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
      if (!activeSettings.enabled) {
        xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SPOOLMAN_READY |
                                                       rtos::EVENT_SPOOLMAN_TAG_FIELD_READY);
      } else {
        // Ohne diesen Aufruf blieben EVENT_SPOOLMAN_READY/
        // EVENT_SPOOLMAN_TAG_FIELD_READY nach jedem Neustart ungesetzt, bis
        // der Nutzer manuell "Verbindung testen" in den Einstellungen
        // drueckt -- alle Tag-Zuordnungsaktionen blieben bis dahin
        // faelschlich deaktiviert, obwohl Spoolman laengst erreichbar war.
        // ApplyConfiguration wird sowohl beim Start (gespeicherte
        // Einstellungen) als auch nach dem Speichern neuer Einstellungen
        // gesendet -- healthCheck() nutzt dieselben command.settings/
        // command.requestId wie ein manueller Verbindungstest.
        healthCheck(ctx, command);
      }
    } else if (command.type == rtos::SpoolmanCommandType::HealthCheck) {
      healthCheck(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::EnsureTagExtraField ||
               command.type == rtos::SpoolmanCommandType::FindSpoolByTag ||
               command.type == rtos::SpoolmanCommandType::SetSpoolTag ||
               command.type == rtos::SpoolmanCommandType::ClearSpoolTag) {
      executeTagClientCommand(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::LoadSpool ||
               command.type == rtos::SpoolmanCommandType::SearchSpools) {
      loadSpools(ctx, command);
    } else if (command.type == rtos::SpoolmanCommandType::LoadFilament) {
      loadFilamentDetails(ctx, command);
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
