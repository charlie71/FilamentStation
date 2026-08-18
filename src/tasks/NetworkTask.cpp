#include <Arduino.h>
#include <WiFiManager.h>

#include <cstdio>
#include <cstring>

#include "config/NetworkConfig.h"
#include "tasks/Tasks.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {

namespace {

struct PortalCredentials {
  char ssid[33]{};
  char password[16]{};
};

enum class WifiSignal : std::uint8_t {
  StationConnected,
  GotIp,
  Disconnected,
  LostIp,
};
static_assert(sizeof(WifiSignal) == sizeof(std::uint8_t));

QueueHandle_t wifiEventQueue = nullptr;
volatile bool wifiEventQueueOverflow = false;

void wifiEventCallback(arduino_event_id_t event) {
  WifiSignal signal{};
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      signal = WifiSignal::StationConnected;
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      signal = WifiSignal::GotIp;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      signal = WifiSignal::Disconnected;
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

rtos::AppEvent networkEvent{};

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
    rtos::logLine("NetworkTask: appEventQueue timeout/overflow");
  }
}

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

void connectOrStartPortal(rtos::RtosContext& ctx, WiFiManager& manager,
                          const PortalCredentials& credentials,
                          const models::NetworkSettings& settings,
                          std::uint32_t requestId,
                          TickType_t& portalStartedAt,
                          std::uint32_t& portalRequestId) {
  rtos::logf("NetworkTask: connecting; portal SSID=%s, timeout=%lus",
             credentials.ssid,
             static_cast<unsigned long>(settings.portalTimeoutSeconds));
  const bool connected = manager.autoConnect(credentials.ssid,
                                              credentials.password);
  if (connected) {
    rtos::logLine("NetworkTask: WiFi connected");
    return;
  }
  if (manager.getConfigPortalActive()) {
    portalStartedAt = xTaskGetTickCount();
    portalRequestId = requestId;
    rtos::logLine("NetworkTask: captive portal active");
    publishPortalStarted(ctx, requestId, credentials, settings);
    return;
  }
  rtos::logLine("NetworkTask: WiFi connection failed");
  publishEvent(ctx, rtos::AppEventType::WifiDisconnected, requestId,
               "WLAN-Verbindung konnte nicht hergestellt werden");
}

void handleWifiSignal(rtos::RtosContext& ctx, WifiSignal signal,
                      std::uint32_t& portalRequestId) {
  switch (signal) {
    case WifiSignal::StationConnected:
      rtos::logLine("NetworkTask: WiFi station connected");
      publishEvent(ctx, rtos::AppEventType::WifiStationConnected,
                   portalRequestId, "WLAN verbunden; IP-Adresse wird bezogen");
      break;
    case WifiSignal::GotIp:
      xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_WIFI_CONNECTED);
      rtos::logLine("NetworkTask: WiFi got IP");
      publishEvent(ctx, rtos::AppEventType::WifiGotIp, portalRequestId,
                   "WLAN-Verbindung und IP-Adresse sind bereit");
      portalRequestId = 0;
      break;
    case WifiSignal::Disconnected:
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_WIFI_CONNECTED);
      rtos::logLine("NetworkTask: WiFi station disconnected");
      publishEvent(ctx, rtos::AppEventType::WifiDisconnected,
                   portalRequestId, "WLAN-Verbindung getrennt");
      break;
    case WifiSignal::LostIp:
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_WIFI_CONNECTED);
      rtos::logLine("NetworkTask: WiFi lost IP");
      publishEvent(ctx, rtos::AppEventType::WifiLostIp, portalRequestId,
                   "WLAN-IP-Adresse verloren");
      break;
  }
}

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
  rtos::logf("NetworkTask: captive portal started; SSID=%s, timeout=%lus",
             credentials.ssid,
             static_cast<unsigned long>(settings.portalTimeoutSeconds));
  manager.startConfigPortal(credentials.ssid, credentials.password);
  if (!manager.getConfigPortalActive()) {
    rtos::logLine("NetworkTask: captive portal start failed");
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
    rtos::logLine("NetworkTask: WiFi event queue/set unavailable");
    vTaskDelete(nullptr);
    return;
  }

  WiFi.onEvent(wifiEventCallback);
  static WiFiManager manager;
  models::NetworkSettings settings{};
  PortalCredentials credentials{};
  bool configurationApplied = false;
  TickType_t portalStartedAt = 0;
  std::uint32_t portalRequestId = 0;

  rtos::NetworkCommand command{};
  for (;;) {
    const bool portalActive = manager.getConfigPortalActive();
    const TickType_t wait =
        portalActive
            ? pdMS_TO_TICKS(config::kWifiPortalServiceIntervalMs)
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
            rtos::logLine("NetworkTask: invalid network configuration rejected");
            publishEvent(ctx, rtos::AppEventType::WifiDisconnected,
                         command.requestId,
                         "Netzwerkkonfiguration konnte nicht angewendet werden");
            break;
          }
          rtos::logf("NetworkTask: configuration applied; hostname=%s, mode=%s",
                     settings.hostname, settings.dhcp ? "DHCP" : "static");
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
          rtos::logLine("NetworkTask: captive portal stopped");
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
          rtos::logLine("NetworkTask: stored WiFi credentials cleared");
          publishEvent(ctx, rtos::AppEventType::WifiCredentialsCleared,
                       command.requestId,
                       "Gespeicherte WLAN-Zugangsdaten wurden gel\xC3\xB6scht");
          break;
      }
    } else if (ready == wifiEventQueue) {
      WifiSignal signal{};
      if (xQueueReceive(wifiEventQueue, &signal, 0) == pdTRUE) {
        handleWifiSignal(ctx, signal, portalRequestId);
      }
    }

    if (wifiEventQueueOverflow) {
      wifiEventQueueOverflow = false;
      rtos::logLine("NetworkTask: WiFi event queue overflow");
    }

    if (!manager.getConfigPortalActive()) continue;

    const bool connected = manager.process();
    if (connected) {
      rtos::logLine("NetworkTask: WiFi connected via captive portal");
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
      rtos::logLine("NetworkTask: captive portal timed out");
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
