#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "rtos/Events.h"
#include "rtos/RtosContext.h"

#ifndef PIO_UNIT_TESTING
namespace {
void haltStartup(const char* reason) {
  Serial.printf("FATAL: %s\n", reason);
  auto& ctx = filament_station::rtos::context();
  if (ctx.systemEventGroup != nullptr) {
    xEventGroupSetBits(ctx.systemEventGroup, filament_station::rtos::EVENT_FATAL_ERROR);
  }
  for (;;) { vTaskDelay(portMAX_DELAY); }
}
}  // namespace

void setup() {
  using namespace filament_station;
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
  Serial.printf("%s %s starting\n", config::kApplicationName, config::kApplicationVersion);
  Serial.printf("Chip: %s, revision %u, %u cores\n", ESP.getChipModel(),
                chipInfo.revision, chipInfo.cores);
  Serial.printf("Heap: %u bytes free\n", ESP.getFreeHeap());
  Serial.printf("PSRAM: %u bytes total, %u bytes free\n", ESP.getPsramSize(), ESP.getFreePsram());
  Serial.flush();
  if (!ctx.createServiceTasks()) { haltStartup("service task creation failed"); }

  Serial.println("RTOS infrastructure started");
}

void loop() { vTaskDelay(portMAX_DELAY); }
#endif
