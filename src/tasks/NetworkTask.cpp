#include <Arduino.h>
#include <WiFiManager.h>

#include <cstdio>

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
  manager.setConfigPortalTimeout(config::kWifiPortalTimeoutSeconds);
  manager.setConnectTimeout(config::kWifiConnectTimeoutSeconds);
}

bool connectOrStartPortal(WiFiManager& manager,
                          const PortalCredentials& credentials) {
  rtos::logf("NetworkTask: connecting; portal SSID=%s, timeout=%lus",
             credentials.ssid,
             static_cast<unsigned long>(config::kWifiPortalTimeoutSeconds));
  const bool connected = manager.autoConnect(credentials.ssid,
                                              credentials.password);
  rtos::logLine(connected ? "NetworkTask: WiFi connected"
                          : "NetworkTask: WiFi connection/portal timed out");
  return connected;
}

void startPortal(WiFiManager& manager,
                 const PortalCredentials& credentials) {
  rtos::logf("NetworkTask: captive portal started; SSID=%s, timeout=%lus",
             credentials.ssid,
             static_cast<unsigned long>(config::kWifiPortalTimeoutSeconds));
  const bool connected = manager.startConfigPortal(credentials.ssid,
                                                    credentials.password);
  rtos::logLine(connected ? "NetworkTask: captive portal connected"
                          : "NetworkTask: captive portal timed out");
}

}  // namespace

void networkTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  WiFiManager manager;
  configureManager(manager);
  const PortalCredentials credentials = makePortalCredentials();

  connectOrStartPortal(manager, credentials);

  rtos::NetworkCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.networkCommandQueue, &command, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    switch (command.type) {
      case rtos::NetworkCommandType::Connect:
        connectOrStartPortal(manager, credentials);
        break;
      case rtos::NetworkCommandType::Reconfigure:
      case rtos::NetworkCommandType::StartPortal:
        startPortal(manager, credentials);
        break;
      case rtos::NetworkCommandType::ClearCredentials:
        manager.resetSettings();
        rtos::logLine("NetworkTask: stored WiFi credentials cleared");
        break;
    }
  }
}
}  // namespace filament_station::tasks
