#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "rtos/Events.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"

#ifndef PIO_UNIT_TESTING
namespace {
void haltStartup(const char* reason) {
  auto& ctx = filament_station::rtos::context();
  FS_LOGE(filament_station::services::LogComponent::Rtos,
          "Startup halted reason=\"%s\"", reason);
  if (ctx.systemEventGroup != nullptr) {
    xEventGroupSetBits(ctx.systemEventGroup, filament_station::rtos::EVENT_FATAL_ERROR);
  }
  for (;;) { vTaskDelay(portMAX_DELAY); }
}
}  // namespace

void setup() {
  using namespace filament_station;
  // Buffer complete log bursts in USB-CDC. This must be configured before
  // begin(); otherwise long NFC diagnostics can be split or lost by HWCDC.
  Serial.setTxBufferSize(4096);
  Serial.begin(config::kSerialBaudRate);
  Serial.setTxTimeoutMs(config::kUsbCdcTransmitTimeoutMs);
  auto& ctx = rtos::context();
  if (!ctx.createObjects()) { haltStartup("RTOS object creation failed"); }
  if (!ctx.createUiTask()) { haltStartup("UiTask creation failed"); }

  // USB-CDC needs time for host enumeration. The UiTask is already running,
  // so the boot screen is visible instead of leaving the display blank.
  vTaskDelay(pdMS_TO_TICKS(config::kUsbCdcStartupDelayMs));

  esp_chip_info_t chipInfo{};
  esp_chip_info(&chipInfo);
  FS_LOGI(services::LogComponent::App, "System starting app=%s firmware=%s",
          config::kApplicationName, config::kApplicationVersion);
  FS_LOGI(services::LogComponent::Rtos,
          "Chip detected model=%s revision=%u cores=%u",
          ESP.getChipModel(), chipInfo.revision, chipInfo.cores);
  FS_LOGI(services::LogComponent::Rtos, "Heap available free_bytes=%u",
          ESP.getFreeHeap());
  FS_LOGI(services::LogComponent::Rtos,
          "PSRAM available total_bytes=%u free_bytes=%u", ESP.getPsramSize(),
          ESP.getFreePsram());
  if (!ctx.createServiceTasks()) { haltStartup("service task creation failed"); }

  FS_LOGI(services::LogComponent::Rtos, "Infrastructure started");
}

void loop() { vTaskDelay(portMAX_DELAY); }
#endif
