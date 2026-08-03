#include "tasks/Tasks.h"

#include <SD.h>
#include <SPI.h>
#include <cstdio>

#include "config/BoardConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
namespace {

void sendStorageEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                      const char* text) {
  rtos::AppEvent event{};
  event.type = type;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    rtos::logLine("StorageTask: appEventQueue timeout/overflow");
  }
}

bool cardIsAccessible() {
  if (SD.cardType() == CARD_NONE) {
    return false;
  }

  File root = SD.open("/");
  if (!root) {
    return false;
  }
  root.close();
  return true;
}

}  // namespace

void storageTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  SPIClass sdSpi(FSPI);
  sdSpi.begin(config::kSdClockPin, config::kSdMisoPin, config::kSdMosiPin,
              config::kSdChipSelectPin);

  if (!SD.begin(config::kSdChipSelectPin, sdSpi) || !cardIsAccessible()) {
    xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    sendStorageEvent(ctx, rtos::AppEventType::SdError,
                     "SD card unavailable; restart required");
    rtos::logLine("StorageTask: SD initialization failed");
  } else {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
    sendStorageEvent(ctx, rtos::AppEventType::SdMounted, "SD card mounted");
    rtos::logLine("StorageTask: SD card mounted");
  }

  bool removalLatched =
      (xEventGroupGetBits(ctx.systemEventGroup) & rtos::EVENT_SD_READY) == 0;
  bool reinsertionReported = false;
  rtos::StorageCommand command{};
  for (;;) {
    // Kein Card-Detect vorhanden: Die Queue blockiert zwischen den bewusst
    // langsamen Zugriffsproben. Storage-Kommandos folgen erst in Phase 2.5.
    const BaseType_t received = xQueueReceive(
        ctx.storageCommandQueue, &command,
        pdMS_TO_TICKS(config::kSdHealthCheckIntervalMs));
    if (received == pdTRUE) {
      rtos::logLine(removalLatched
                        ? "StorageTask: command rejected; restart required"
                        : "StorageTask: command deferred until phase 2.5");
    }

    const bool accessible = cardIsAccessible();
    if (!removalLatched && !accessible) {
      removalLatched = true;
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SD_READY);
      sendStorageEvent(ctx, rtos::AppEventType::SdRemoved,
                       "SD card removed; restart required");
      rtos::logLine("StorageTask: SD card removed; error latched");
    } else if (removalLatched && accessible && !reinsertionReported) {
      reinsertionReported = true;
      sendStorageEvent(ctx, rtos::AppEventType::SdReinserted,
                       "SD card reinserted; restart still required");
      rtos::logLine("StorageTask: SD reinserted; restart still required");
    }
  }
}
}  // namespace filament_station::tasks
