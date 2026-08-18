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
  char ssid[32]{};
  char password[16]{};
};

PortalCredentials makePortalCredentials() {
  PortalCredentials credentials{};
  const auto deviceSuffix =
      static_cast<std::uint32_t>(ESP.getEfuseMac() & 0x00FFFFFFULL);
  std::snprintf(credentials.ssid, sizeof(credentials.ssid), "%s-%06lX",
                config::kWifiPortalSsidPrefix,
                static_cast<unsigned long>(deviceSuffix));
  std::snprintf(credentials.password, sizeof(credentials.password), "%s-%06lX",
                config::kWifiPortalPasswordPrefix,
                static_cast<unsigned long>(deviceSuffix));
  return credentials;
}

void configureManager(WiFiManager& manager) {
  manager.setConfigPortalBlocking(false);
  manager.setConfigPortalTimeout(config::kWifiPortalTimeoutSeconds);
  manager.setConnectTimeout(config::kWifiConnectTimeoutSeconds);
}

rtos::AppEvent networkEvent{};

void publishEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                  std::uint32_t requestId, const char* text) {
  std::memset(&networkEvent, 0, sizeof(networkEvent));
  networkEvent.type = type;
  networkEvent.requestId = requestId;
  std::snprintf(networkEvent.text, sizeof(networkEvent.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &networkEvent, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    rtos::logLine("NetworkTask: appEventQueue timeout/overflow");
  }
}

void publishPortalStarted(rtos::RtosContext& ctx, std::uint32_t requestId,
                          const PortalCredentials& credentials) {
  char status[192]{};
  std::snprintf(status, sizeof(status),
                "WLAN: %s\nPasswort: %s\nPortal-Timeout: %lu Sekunden",
                credentials.ssid, credentials.password,
                static_cast<unsigned long>(config::kWifiPortalTimeoutSeconds));
  publishEvent(ctx, rtos::AppEventType::WifiConfigPortalStarted, requestId,
               status);
}

void connectOrStartPortal(rtos::RtosContext& ctx, WiFiManager& manager,
                          const PortalCredentials& credentials,
                          std::uint32_t requestId,
                          TickType_t& portalStartedAt,
                          std::uint32_t& portalRequestId) {
  rtos::logf("NetworkTask: connecting; portal SSID=%s, timeout=%lus",
             credentials.ssid,
             static_cast<unsigned long>(config::kWifiPortalTimeoutSeconds));
  const bool connected = manager.autoConnect(credentials.ssid,
                                              credentials.password);
  if (connected) {
    rtos::logLine("NetworkTask: WiFi connected");
    publishEvent(ctx, rtos::AppEventType::WifiConnected, requestId,
                 "WLAN-Verbindung hergestellt");
    return;
  }
  if (manager.getConfigPortalActive()) {
    portalStartedAt = xTaskGetTickCount();
    portalRequestId = requestId;
    rtos::logLine("NetworkTask: captive portal active");
    publishPortalStarted(ctx, requestId, credentials);
    return;
  }
  rtos::logLine("NetworkTask: WiFi connection failed");
  publishEvent(ctx, rtos::AppEventType::WifiDisconnected, requestId,
               "WLAN-Verbindung konnte nicht hergestellt werden");
}

void startPortal(rtos::RtosContext& ctx, WiFiManager& manager,
                 const PortalCredentials& credentials,
                 std::uint32_t requestId, TickType_t& portalStartedAt,
                 std::uint32_t& portalRequestId) {
  if (manager.getConfigPortalActive()) {
    portalRequestId = requestId;
    publishPortalStarted(ctx, requestId, credentials);
    return;
  }
  rtos::logf("NetworkTask: captive portal started; SSID=%s, timeout=%lus",
             credentials.ssid,
             static_cast<unsigned long>(config::kWifiPortalTimeoutSeconds));
  manager.startConfigPortal(credentials.ssid, credentials.password);
  if (!manager.getConfigPortalActive()) {
    rtos::logLine("NetworkTask: captive portal start failed");
    publishEvent(ctx, rtos::AppEventType::WifiDisconnected, requestId,
                 "WLAN-Konfigurationsportal konnte nicht gestartet werden");
    return;
  }
  portalStartedAt = xTaskGetTickCount();
  portalRequestId = requestId;
  publishPortalStarted(ctx, requestId, credentials);
}

}  // namespace

void networkTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  static WiFiManager manager;
  configureManager(manager);
  static const PortalCredentials credentials = makePortalCredentials();
  TickType_t portalStartedAt = 0;
  std::uint32_t portalRequestId = 0;

  connectOrStartPortal(ctx, manager, credentials, 0, portalStartedAt,
                       portalRequestId);

  rtos::NetworkCommand command{};
  for (;;) {
    const bool portalActive = manager.getConfigPortalActive();
    const TickType_t wait =
        portalActive
            ? pdMS_TO_TICKS(config::kWifiPortalServiceIntervalMs)
            : portMAX_DELAY;
    if (xQueueReceive(ctx.networkCommandQueue, &command, wait) == pdTRUE) {
      switch (command.type) {
        case rtos::NetworkCommandType::Connect:
          connectOrStartPortal(ctx, manager, credentials, command.requestId,
                               portalStartedAt, portalRequestId);
          break;
        case rtos::NetworkCommandType::Reconfigure:
        case rtos::NetworkCommandType::StartPortal:
          startPortal(ctx, manager, credentials, command.requestId,
                      portalStartedAt, portalRequestId);
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
          rtos::logLine("NetworkTask: stored WiFi credentials cleared");
          break;
      }
    }

    if (!manager.getConfigPortalActive()) continue;

    const bool connected = manager.process();
    if (connected) {
      rtos::logLine("NetworkTask: WiFi connected via captive portal");
      publishEvent(ctx, rtos::AppEventType::WifiConnected, portalRequestId,
                   "WLAN-Verbindung hergestellt");
      portalStartedAt = 0;
      portalRequestId = 0;
      continue;
    }

    const TickType_t timeoutTicks =
        pdMS_TO_TICKS(config::kWifiPortalTimeoutSeconds * 1000UL);
    if (!manager.getConfigPortalActive() ||
        static_cast<TickType_t>(xTaskGetTickCount() - portalStartedAt) >=
            timeoutTicks) {
      if (manager.getConfigPortalActive()) manager.stopConfigPortal();
      rtos::logLine("NetworkTask: captive portal timed out");
      publishEvent(ctx, rtos::AppEventType::WifiConfigPortalTimedOut,
                   portalRequestId,
                   "WLAN-Konfigurationsportal wurde wegen ZeitÃ¼" "berschreitung beendet");
      portalStartedAt = 0;
      portalRequestId = 0;
    }
  }
}
}  // namespace filament_station::tasks
