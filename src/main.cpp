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
  auto& ctx = filament_station::rtos::context();
  if (ctx.logQueue != nullptr && ctx.loggingTask != nullptr) {
    filament_station::rtos::logf("FATAL: %s", reason);
  } else {
    Serial.printf("FATAL: %s\n", reason);
  }
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
  rtos::logf("%s %s starting", config::kApplicationName,
             config::kApplicationVersion);
  rtos::logf("Chip: %s, revision %u, %u cores", ESP.getChipModel(),
             chipInfo.revision, chipInfo.cores);
  rtos::logf("Heap: %u bytes free", ESP.getFreeHeap());
  rtos::logf("PSRAM: %u bytes total, %u bytes free", ESP.getPsramSize(),
             ESP.getFreePsram());
  if (!ctx.createServiceTasks()) { haltStartup("service task creation failed"); }

  rtos::logLine("RTOS infrastructure started");
}

void loop() { vTaskDelay(portMAX_DELAY); }
#endif
