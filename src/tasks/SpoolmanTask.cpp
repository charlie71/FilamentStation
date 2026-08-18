#include "tasks/Tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "models/SpoolmanSpool.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

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

void sendSpool(rtos::RtosContext& ctx, std::uint32_t requestId,
               std::int32_t index, const models::SpoolmanSpool& spool) {
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::SpoolmanResponse;
  event.requestId = requestId;
  event.value = index;
  event.spoolId = spool.id;
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
    } else if (command.type == rtos::SpoolmanCommandType::ImportTagDefinition) {
      sendResult(ctx, rtos::AppEventType::SpoolmanError, command.requestId,
                 "Spoolman-Import ist erst in einer sp\xC3\xA4teren Phase verf\xC3\xBCgbar");
    }
  }
}
}  // namespace filament_station::tasks
