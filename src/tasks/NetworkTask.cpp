/**
 * @file
 * @brief Implements tasks::networkTask(): WiFiManager-based station
 *        connection/reconnect and captive-portal lifecycle.
 */
#include <Arduino.h>
#include <WiFiManager.h>

#include <cstdio>
#include <cstring>

#include "config/NetworkConfig.h"
#include "tasks/Tasks.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"

namespace filament_station::tasks {

namespace {

/// @brief The generated captive-portal SSID/password for this device.
struct PortalCredentials {
  char ssid[33]{};      ///< Portal SSID, derived from settings.portalName plus a MAC-derived suffix.
  char password[16]{};  ///< Portal password, derived from config::kWifiPortalPasswordPrefix plus the same suffix.
};

/// @brief Simplified WiFi station event, forwarded from the Arduino WiFi
///        event callback (which runs on a different task) to networkTask()
///        via #wifiEventQueue.
enum class WifiSignal : std::uint8_t {
  StationConnected,  ///< Associated with the AP, IP not yet assigned.
  GotIp,              ///< IP address acquired; connection is fully usable.
  Disconnected,       ///< Lost the AP association.
  LostIp,             ///< Still associated, but the IP address was lost.
  // Nutzerbericht 2026-08-30 (Fortsetzung): der bisherige feste
  // Settle-Delay (kWifiPostWakeSettleMs) nach dem Aufwachen löste den
  // Reconnect-Fehlschlag nicht zuverlässig -- Verdacht war richtig
  // (WiFi.mode(WIFI_STA) noch nicht wirklich bereit), aber eine feste
  // Wartezeit ist ein Ratespiel. Reagiert jetzt stattdessen auf das
  // tatsächliche STA_START-Ereignis (siehe wifiEventCallback()), mit
  // Fallback-Timeout falls das Ereignis ausbleibt.
  StationStarted,     ///< STA-Schnittstelle ist bereit (ARDUINO_EVENT_WIFI_STA_START), erst danach ist WiFi.reconnect() zuverlässig.
};
static_assert(sizeof(WifiSignal) == sizeof(std::uint8_t));

QueueHandle_t wifiEventQueue = nullptr;  ///< Copy of rtos::RtosContext::wifiEventQueue, set at task startup for wifiEventCallback() to use.
volatile bool wifiEventQueueOverflow = false;  ///< Set by wifiEventCallback() when #wifiEventQueue is full; polled and logged once per occurrence by networkTask().
// Nur für Diagnose (Nutzerbericht 2026-08-30): der bisherige
// "WiFi station disconnected"-Log verriet nie, WARUM -- wifi_err_reason_t
// als roher Zahlenwert (u. a. 2=AUTH_EXPIRE, 201=NO_AP_FOUND,
// 202=AUTH_FAIL, 204=HANDSHAKE_TIMEOUT, siehe esp_wifi_types.h), aus dem
// vollen WiFiEventFuncCb-Callback gelesen (die bisherige Callback-Variante
// bekam nur die Event-ID, nicht die Zusatzinfo). Nicht synchronisiert
// (volatile genügt, reiner Diagnosewert, keine Ablaufsteuerung hängt
// daran) -- gleiches Muster wie #wifiEventQueueOverflow.
volatile std::uint8_t lastWifiDisconnectReason = 0;  ///< Raw wifi_err_reason_t of the most recent STA disconnect, for diagnostic logging only.

/// @brief Arduino WiFi event callback; classifies and forwards relevant events to #wifiEventQueue.
/// @param event Raw Arduino WiFi event id.
/// @param info Event-specific details (only the disconnect reason is used here).
/// @note Runs on the Arduino WiFi event task, not an ISR; kept bounded (one enqueue, no blocking).
void wifiEventCallback(arduino_event_id_t event, arduino_event_info_t info) {
  WifiSignal signal{};
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      signal = WifiSignal::StationStarted;
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      signal = WifiSignal::StationConnected;
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      signal = WifiSignal::GotIp;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      signal = WifiSignal::Disconnected;
      lastWifiDisconnectReason = info.wifi_sta_disconnected.reason;
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      signal = WifiSignal::LostIp;
      break;
    default:
      return;
  }
  // Arduino-ESP32 dispatches WiFi callbacks from its event task, not from an
  // ISR. Keep the callback bounded: copy one small enum and return.
  if (xQueueSend(wifiEventQueue, &signal, 0) != pdPASS) {
    wifiEventQueueOverflow = true;
  }
}

/// @brief Derives the captive-portal SSID/password from settings and the device's MAC.
/// @param settings Network settings supplying the portal name.
/// @return The generated credentials.
PortalCredentials makePortalCredentials(
    const models::NetworkSettings& settings) {
  PortalCredentials credentials{};
  const auto deviceSuffix =
      static_cast<std::uint32_t>(ESP.getEfuseMac() & 0x00FFFFFFULL);
  std::snprintf(credentials.ssid, sizeof(credentials.ssid), "%s-%06lX",
                settings.portalName,
                static_cast<unsigned long>(deviceSuffix));
  std::snprintf(credentials.password, sizeof(credentials.password), "%s-%06lX",
                config::kWifiPortalPasswordPrefix,
                static_cast<unsigned long>(deviceSuffix));
  return credentials;
}

/// @brief Applies network settings (hostname, timeouts, static IP if not DHCP) to WiFiManager.
/// @param manager WiFiManager instance to configure.
/// @param settings Settings to apply.
/// @return false if the hostname was rejected, or (for static IP) any address failed to parse.
bool configureManager(WiFiManager& manager,
                      const models::NetworkSettings& settings) {
  manager.setConfigPortalBlocking(false);
  manager.setConfigPortalTimeout(settings.portalTimeoutSeconds);
  manager.setConnectTimeout(settings.connectTimeoutSeconds);
  if (!manager.setHostname(settings.hostname)) return false;
  if (settings.dhcp) return true;

  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  if (!ip.fromString(settings.ipAddress) ||
      !gateway.fromString(settings.gateway) ||
      !subnet.fromString(settings.subnetMask) ||
      (settings.dns[0] != '\0' && !dns.fromString(settings.dns))) {
    return false;
  }
  manager.setSTAStaticIPConfig(ip, gateway, subnet, dns);
  return true;
}

rtos::AppEvent networkEvent{};  ///< Reused scratch AppEvent for publishEvent(), avoiding a large stack allocation per call.

/// @brief Fills in and sends a WiFi-status AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param requestId Correlation id.
/// @param text Status text.
/// @param ssidOverride SSID to report instead of the current station SSID, or null.
/// @param ipOverride IP to report instead of the current station IP, or null.
void publishEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                  std::uint32_t requestId, const char* text,
                  const char* ssidOverride = nullptr,
                  const char* ipOverride = nullptr) {
  std::memset(&networkEvent, 0, sizeof(networkEvent));
  networkEvent.type = type;
  networkEvent.requestId = requestId;
  networkEvent.value = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  const String stationSsid = WiFi.SSID();
  const String stationIp = WiFi.localIP().toString();
  std::snprintf(networkEvent.networkSsid,
                sizeof(networkEvent.networkSsid), "%s",
                ssidOverride != nullptr ? ssidOverride : stationSsid.c_str());
  std::snprintf(networkEvent.networkIp, sizeof(networkEvent.networkIp), "%s",
                ipOverride != nullptr ? ipOverride : stationIp.c_str());
  std::snprintf(networkEvent.text, sizeof(networkEvent.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &networkEvent, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Net,
            "Event enqueue failed queue=app_event event=%u request_id=%lu",
            static_cast<unsigned>(type),
            static_cast<unsigned long>(requestId));
  }
}

/// @brief Publishes a WifiConfigPortalStarted event with the portal's SSID/password/AP-IP.
/// @param ctx Owning RTOS context.
/// @param requestId Correlation id.
/// @param credentials Portal credentials to report.
/// @param settings Settings supplying the portal timeout to report.
void publishPortalStarted(rtos::RtosContext& ctx, std::uint32_t requestId,
                          const PortalCredentials& credentials,
                          const models::NetworkSettings& settings) {
  char status[192]{};
  std::snprintf(status, sizeof(status),
                "WLAN: %s\nPasswort: %s\nPortal-Timeout: %lu Sekunden",
                credentials.ssid, credentials.password,
                static_cast<unsigned long>(settings.portalTimeoutSeconds));
  publishEvent(ctx, rtos::AppEventType::WifiConfigPortalStarted, requestId,
               status, credentials.ssid, WiFi.softAPIP().toString().c_str());
}

/// @brief Attempts a station connection via WiFiManager::autoConnect(),
///        falling back to the captive portal on failure.
/// @param ctx Owning RTOS context.
/// @param manager WiFiManager instance to drive.
/// @param credentials Portal credentials to use if a portal is started.
/// @param settings Settings supplying the portal timeout.
/// @param requestId Correlation id.
/// @param portalStartedAt Out parameter set to the current tick count if a portal was started.
/// @param portalRequestId Out parameter set to `requestId` if a portal was started.
void connectOrStartPortal(rtos::RtosContext& ctx, WiFiManager& manager,
                          const PortalCredentials& credentials,
                          const models::NetworkSettings& settings,
                          std::uint32_t requestId,
                          TickType_t& portalStartedAt,
                          std::uint32_t& portalRequestId) {
  FS_LOGI(services::LogComponent::Net,
          "Connection attempt portal_ssid=\"%s\" timeout_s=%lu",
          credentials.ssid,
          static_cast<unsigned long>(settings.portalTimeoutSeconds));
  const bool connected = manager.autoConnect(credentials.ssid,
                                              credentials.password);
  if (connected) {
    FS_LOGI(services::LogComponent::Net, "WiFi connected");
    return;
  }
  if (manager.getConfigPortalActive()) {
    portalStartedAt = xTaskGetTickCount();
    portalRequestId = requestId;
    FS_LOGI(services::LogComponent::Net, "Captive portal active");
    publishPortalStarted(ctx, requestId, credentials, settings);
    return;
  }
  FS_LOGW(services::LogComponent::Net, "WiFi connection failed");
  publishEvent(ctx, rtos::AppEventType::WifiDisconnected, requestId,
               "WLAN-Verbindung konnte nicht hergestellt werden");
}

/// @brief Applies one WifiSignal: updates the ready event bit and publishes the corresponding AppEvent.
/// @param ctx Owning RTOS context.
/// @param signal Signal to handle.
/// @param portalRequestId Correlation id to report; cleared once GotIp is reached.
void handleWifiSignal(rtos::RtosContext& ctx, WifiSignal signal,
                      std::uint32_t& portalRequestId) {
  switch (signal) {
    case WifiSignal::StationConnected:
      FS_LOGI(services::LogComponent::Net, "WiFi station connected");
      publishEvent(ctx, rtos::AppEventType::WifiStationConnected,
                   portalRequestId, "WLAN verbunden; IP-Adresse wird bezogen");
      break;
    case WifiSignal::GotIp:
      xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_WIFI_CONNECTED);
      FS_LOGI(services::LogComponent::Net, "WiFi address acquired ip=%s",
              WiFi.localIP().toString().c_str());
      publishEvent(ctx, rtos::AppEventType::WifiGotIp, portalRequestId,
                   "WLAN-Verbindung und IP-Adresse sind bereit");
      portalRequestId = 0;
      break;
    case WifiSignal::Disconnected:
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_WIFI_CONNECTED);
      FS_LOGW(services::LogComponent::Net,
              "WiFi station disconnected reason=%u",
              static_cast<unsigned>(lastWifiDisconnectReason));
      publishEvent(ctx, rtos::AppEventType::WifiDisconnected,
                   portalRequestId, "WLAN-Verbindung getrennt");
      break;
    case WifiSignal::StationStarted:
      FS_LOGD(services::LogComponent::Net, "WiFi STA interface started");
      break;
    case WifiSignal::LostIp:
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_WIFI_CONNECTED);
      FS_LOGW(services::LogComponent::Net, "WiFi address lost");
      publishEvent(ctx, rtos::AppEventType::WifiLostIp, portalRequestId,
                   "WLAN-IP-Adresse verloren");
      break;
  }
}

/// @brief Sends a PowerDownAcknowledged command to PowerTask.
/// @param ctx Owning RTOS context.
void sendPowerAck(rtos::RtosContext& ctx) {
  rtos::PowerCommand command{};
  command.type = rtos::PowerCommandType::PowerDownAcknowledged;
  command.source = rtos::PowerPeripheral::Network;
  if (xQueueSend(ctx.powerCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Net,
            "Event enqueue failed queue=power_command op=power_down_ack");
  }
}

/// @brief Explicitly starts (or reports the already-active) captive portal.
/// @param ctx Owning RTOS context.
/// @param manager WiFiManager instance to drive.
/// @param credentials Portal credentials to use.
/// @param settings Settings supplying the portal timeout.
/// @param requestId Correlation id.
/// @param portalStartedAt Out parameter set to the current tick count.
/// @param portalRequestId Out parameter set to `requestId`.
void startPortal(rtos::RtosContext& ctx, WiFiManager& manager,
                 const PortalCredentials& credentials,
                 const models::NetworkSettings& settings,
                 std::uint32_t requestId, TickType_t& portalStartedAt,
                 std::uint32_t& portalRequestId) {
  if (manager.getConfigPortalActive()) {
    portalRequestId = requestId;
    publishPortalStarted(ctx, requestId, credentials, settings);
    return;
  }
  FS_LOGI(services::LogComponent::Net,
          "Captive portal starting ssid=\"%s\" timeout_s=%lu",
          credentials.ssid,
          static_cast<unsigned long>(settings.portalTimeoutSeconds));
  manager.startConfigPortal(credentials.ssid, credentials.password);
  if (!manager.getConfigPortalActive()) {
    FS_LOGE(services::LogComponent::Net, "Captive portal start failed");
    publishEvent(ctx, rtos::AppEventType::WifiDisconnected, requestId,
                 "WLAN-Konfigurationsportal konnte nicht gestartet werden");
    return;
  }
  portalStartedAt = xTaskGetTickCount();
  portalRequestId = requestId;
  publishPortalStarted(ctx, requestId, credentials, settings);
}

}  // namespace

void networkTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  wifiEventQueue = ctx.wifiEventQueue;
  QueueSetHandle_t queueSet = ctx.networkQueueSet;
  if (wifiEventQueue == nullptr || queueSet == nullptr) {
    FS_LOGE(services::LogComponent::Net,
            "Initialization failed reason=event_queue_unavailable");
    vTaskDelete(nullptr);
    return;
  }

  WiFi.onEvent(wifiEventCallback);
  static WiFiManager manager;
  // WiFiManager 2.0.17 exposes this as its official runtime switch. Its own
  // *wm: output bypasses the application logger and may include configuration
  // details, so keep it disabled and report relevant states through [NET].
  manager.setDebugOutput(false);
  models::NetworkSettings settings{};
  PortalCredentials credentials{};
  bool configurationApplied = false;
  TickType_t portalStartedAt = 0;
  std::uint32_t portalRequestId = 0;
  // See kWifiReconnectIntervalMs: WiFiManager's own ESP32 auto-reconnect is
  // compiled out in this project, so a runtime WiFi loss is otherwise never
  // retried. 0 allows an immediate first attempt.
  TickType_t lastReconnectAttemptAt = 0;
  // Energiesparen (TASKS.md Phase 11.5): während absichtlich abgeschaltetem
  // WiFi soll der bestehende Reconnect-Mechanismus nicht dagegen ankämpfen.
  bool poweredDown = false;
  // Nutzerbericht 2026-08-30 (Fortsetzung, der feste kWifiPostWakeSettleMs-
  // Delay reichte nicht zuverlässig): nach WiFi.mode(WIFI_STA) im
  // Aufwach-Pfad wird jetzt auf das echte STA_START-Ereignis gewartet,
  // statt eine feste Zeit zu raten -- awaitingStaStart/staStartFallbackAt
  // steuern das, siehe PowerUp/StationStarted unten. Fallback-Deadline
  // verhindert ein permanentes Hängenbleiben, falls das Ereignis auf
  // dieser Hardware/Firmware-Version doch einmal ausbleiben sollte.
  bool awaitingStaStart = false;
  TickType_t staStartFallbackAt = 0;

  rtos::NetworkCommand command{};
  for (;;) {
    const bool portalActive = manager.getConfigPortalActive();
    const bool disconnectedAndConfigured = !poweredDown && !portalActive &&
        configurationApplied && WiFi.status() != WL_CONNECTED;
    const TickType_t wait =
        portalActive
            ? pdMS_TO_TICKS(config::kWifiPortalServiceIntervalMs)
        : awaitingStaStart
            ? pdMS_TO_TICKS(config::kWifiStaStartPollIntervalMs)
        : disconnectedAndConfigured
            ? pdMS_TO_TICKS(config::kWifiReconnectIntervalMs)
            : portMAX_DELAY;
    const QueueSetMemberHandle_t ready = xQueueSelectFromSet(queueSet, wait);
    if (ready == ctx.networkCommandQueue &&
        xQueueReceive(ctx.networkCommandQueue, &command, 0) == pdTRUE) {
      switch (command.type) {
        case rtos::NetworkCommandType::ApplyConfiguration:
          settings = command.settings;
          credentials = makePortalCredentials(settings);
          configurationApplied = configureManager(manager, settings);
          if (!configurationApplied) {
            FS_LOGE(services::LogComponent::Net,
                    "Configuration rejected reason=invalid");
            publishEvent(ctx, rtos::AppEventType::WifiDisconnected,
                         command.requestId,
                         "Netzwerkkonfiguration konnte nicht angewendet werden");
            break;
          }
          FS_LOGI(services::LogComponent::Net,
                  "Configuration applied hostname=\"%s\" mode=%s",
                  settings.hostname, settings.dhcp ? "dhcp" : "static");
          connectOrStartPortal(ctx, manager, credentials, settings,
                               command.requestId, portalStartedAt,
                               portalRequestId);
          break;
        case rtos::NetworkCommandType::RequestStatus:
          if (manager.getConfigPortalActive()) {
            publishPortalStarted(ctx, 0, credentials, settings);
          } else if (WiFi.status() == WL_CONNECTED) {
            publishEvent(ctx, rtos::AppEventType::WifiGotIp, 0,
                         "WLAN-Verbindung und IP-Adresse sind bereit");
          } else {
            publishEvent(ctx, rtos::AppEventType::WifiDisconnected, 0,
                         "WLAN ist nicht verbunden");
          }
          break;
        case rtos::NetworkCommandType::Connect:
          if (configurationApplied)
            connectOrStartPortal(ctx, manager, credentials, settings,
                                 command.requestId, portalStartedAt,
                                 portalRequestId);
          break;
        case rtos::NetworkCommandType::Reconfigure:
        case rtos::NetworkCommandType::StartPortal:
          if (configurationApplied)
            startPortal(ctx, manager, credentials, settings,
                        command.requestId, portalStartedAt, portalRequestId);
          break;
        case rtos::NetworkCommandType::StopPortal:
          if (manager.getConfigPortalActive()) manager.stopConfigPortal();
          FS_LOGI(services::LogComponent::Net, "Captive portal stopped");
          publishEvent(ctx, rtos::AppEventType::WifiConfigPortalStopped,
                       command.requestId,
                       "WLAN-Konfigurationsportal wurde abgebrochen");
          portalStartedAt = 0;
          portalRequestId = 0;
          break;
        case rtos::NetworkCommandType::ClearCredentials:
          manager.resetSettings();
          xEventGroupClearBits(ctx.systemEventGroup,
                               rtos::EVENT_WIFI_CONNECTED);
          FS_LOGI(services::LogComponent::Net, "WiFi credentials cleared");
          publishEvent(ctx, rtos::AppEventType::WifiCredentialsCleared,
                       command.requestId,
                       "Gespeicherte WLAN-Zugangsdaten wurden gel\xC3\xB6scht");
          break;
        case rtos::NetworkCommandType::PowerDown:
          if (!poweredDown) {
            poweredDown = true;
            WiFi.mode(WIFI_OFF);
            FS_LOGI(services::LogComponent::Net,
                    "WiFi powered off (energy saving)");
            sendPowerAck(ctx);
          }
          break;
        case rtos::NetworkCommandType::PowerUp:
          if (poweredDown) {
            poweredDown = false;
            // Nutzerbericht 2026-08-30 (Fortsetzung): ein fester Delay vor
            // dem ersten reconnect()-Versuch löste das vereinzelte
            // Fehlschlagen nicht zuverlässig -- statt eine Wartezeit zu
            // raten, jetzt auf das tatsächliche STA_START-Ereignis warten
            // (siehe wifiEventCallback()/awaitingStaStart oben);
            // lastReconnectAttemptAt wird erst dort (oder vom
            // Fallback-Timeout weiter unten) auf 0 gesetzt.
            WiFi.mode(WIFI_STA);
            awaitingStaStart = true;
            staStartFallbackAt =
                xTaskGetTickCount() +
                pdMS_TO_TICKS(config::kWifiStaStartFallbackMs);
            FS_LOGI(services::LogComponent::Net,
                    "WiFi powered on, waiting for STA start event");
          }
          break;
      }
    } else if (ready == wifiEventQueue) {
      WifiSignal signal{};
      if (xQueueReceive(wifiEventQueue, &signal, 0) == pdTRUE) {
        if (signal == WifiSignal::StationStarted && awaitingStaStart) {
          awaitingStaStart = false;
          lastReconnectAttemptAt = 0;
          FS_LOGI(services::LogComponent::Net,
                  "STA start event received, reconnect scheduled");
        }
        handleWifiSignal(ctx, signal, portalRequestId);
      }
    }

    if (wifiEventQueueOverflow) {
      wifiEventQueueOverflow = false;
      FS_LOGW(services::LogComponent::Net,
              "Event enqueue failed queue=wifi_event");
    }

    if (awaitingStaStart &&
        static_cast<std::int32_t>(xTaskGetTickCount() - staStartFallbackAt) >=
            0) {
      // Fallback (Nutzerbericht 2026-08-30): STA_START ist auf ESP32
      // normalerweise zuverlässig, aber falls es doch einmal ausbleibt,
      // soll der Reconnect trotzdem irgendwann versucht werden statt für
      // immer zu warten.
      awaitingStaStart = false;
      lastReconnectAttemptAt = 0;
      FS_LOGW(services::LogComponent::Net,
              "STA start event timed out, attempting reconnect anyway");
    }

    if (!awaitingStaStart && !poweredDown && !portalActive &&
        configurationApplied && WiFi.status() != WL_CONNECTED) {
      const TickType_t now = xTaskGetTickCount();
      if (static_cast<TickType_t>(now - lastReconnectAttemptAt) >=
          pdMS_TO_TICKS(config::kWifiReconnectIntervalMs)) {
        lastReconnectAttemptAt = now;
        const wl_status_t statusBefore = WiFi.status();
        const bool reconnectStarted = WiFi.reconnect();
        FS_LOGI(services::LogComponent::Net,
                "Attempting WiFi reconnect after runtime loss "
                "status_before=%d reconnect_started=%d",
                static_cast<int>(statusBefore), reconnectStarted ? 1 : 0);
      }
    }

    if (!manager.getConfigPortalActive()) continue;

    const bool connected = manager.process();
    if (connected) {
      FS_LOGI(services::LogComponent::Net,
              "WiFi connected source=captive_portal");
      portalStartedAt = 0;
      continue;
    }

    const TickType_t timeoutTicks =
        pdMS_TO_TICKS(static_cast<std::uint32_t>(
                          settings.portalTimeoutSeconds) *
                      1000UL);
    if (!manager.getConfigPortalActive() ||
        static_cast<TickType_t>(xTaskGetTickCount() - portalStartedAt) >=
            timeoutTicks) {
      if (manager.getConfigPortalActive()) manager.stopConfigPortal();
      FS_LOGW(services::LogComponent::Net, "Captive portal timed out");
      publishEvent(ctx, rtos::AppEventType::WifiConfigPortalTimedOut,
                   portalRequestId,
                   "WLAN-Konfigurationsportal wurde wegen Zeit\xC3\xBC"
                   "berschreitung beendet");
      portalStartedAt = 0;
      portalRequestId = 0;
    }
  }
}
}  // namespace filament_station::tasks
