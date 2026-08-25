#include "tasks/Tasks.h"

#include <Arduino.h>
#include <cstdio>
#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "config/PowerConfig.h"
#include "config/TaskConfig.h"
#include "drivers/DisplayDriver.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"
#include "ui/UiBridge.h"

namespace filament_station::tasks {
void uiTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  if (!drivers::initializeDisplay()) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    FS_LOGE(services::LogComponent::Display, "Display initialization failed");
    vTaskSuspend(nullptr);
  }
  FS_LOGI(services::LogComponent::Display,
          "Display ready width=480 height=320 rotation=3");

  const std::uint32_t psramBefore = ESP.getFreePsram();
  ui::UiRuntimeInfo runtimeInfo{};
  if (!ui::initializeLvgl(runtimeInfo, ctx)) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    FS_LOGE(services::LogComponent::Ui,
            "LVGL initialization failed reason=allocation_or_setup");
    vTaskSuspend(nullptr);
  }
  const std::uint32_t psramAfter = ESP.getFreePsram();
  const std::uint32_t psramConsumed =
      psramBefore >= psramAfter ? psramBefore - psramAfter : 0;
  FS_LOGI(services::LogComponent::Ui,
          "LVGL ready draw_buffer_bytes=%u draw_buffers=2 psram_used_bytes=%lu psram_free_bytes=%lu",
          static_cast<unsigned int>(runtimeInfo.bytesPerDrawBuffer),
          static_cast<unsigned long>(psramConsumed),
          static_cast<unsigned long>(psramAfter));
  if (!runtimeInfo.drawBuffersInPsram) {
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_FATAL_ERROR);
    FS_LOGE(services::LogComponent::Ui,
            "LVGL draw buffers invalid storage=non_psram");
    vTaskSuspend(nullptr);
  }

  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_UI_READY);

  rtos::AppEvent event{};
  event.type = rtos::AppEventType::UiCommunicationTest;
  event.requestId = config::kCommunicationTestRequestId;
  std::snprintf(event.text, sizeof(event.text),
                "UiTask communication test");
  if (xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(1000)) != pdPASS) {
    FS_LOGW(services::LogComponent::Ui,
            "Event enqueue failed queue=app_event op=communication_test");
  } else {
    FS_LOGD(services::LogComponent::Ui,
            "Communication test sent request_id=%lu",
            static_cast<unsigned long>(event.requestId));
  }

  rtos::UiCommand command{};
  TickType_t lastPowerReportAt = 0;
  for (;;) {
    const std::uint32_t requestedSleepMs = ui::runLvglTimers();
    const std::uint32_t sleepMs =
        requestedSleepMs < config::kLvglMinimumSleepMs
            ? config::kLvglMinimumSleepMs
            : requestedSleepMs;
    // Nach oben auf das Power-Report-Intervall begrenzt, damit die Schleife
    // auch bei voelliger LVGL-Ruhe (sleepMs == UINT32_MAX) regelmaessig
    // genug lv_display_get_inactive_time() an PowerTask meldet.
    const std::uint32_t boundedSleepMs =
        sleepMs > config::kPowerActivityReportIntervalMs
            ? config::kPowerActivityReportIntervalMs
            : sleepMs;
    const TickType_t waitTicks = pdMS_TO_TICKS(boundedSleepMs);
    const TickType_t now = xTaskGetTickCount();
    if (static_cast<TickType_t>(now - lastPowerReportAt) >=
        pdMS_TO_TICKS(config::kPowerActivityReportIntervalMs)) {
      lastPowerReportAt = now;
      rtos::PowerCommand powerCommand{};
      powerCommand.type = rtos::PowerCommandType::ReportInactivity;
      powerCommand.inactiveMs = ui::inputInactiveMs();
      if (xQueueSend(ctx.powerCommandQueue, &powerCommand, 0) != pdPASS) {
        FS_LOGW(services::LogComponent::Ui,
                "Power queue full, dropped inactivity report");
      }
    }
    if (xQueueReceive(ctx.uiCommandQueue, &command,
                      waitTicks) == pdTRUE) {
      do {
        ui::processUiCommand(command);
        // Continuous weight updates are expected and would otherwise flood
        // USB-CDC. User-visible responses and explicit errors remain logged.
        if (command.type != rtos::UiCommandType::UpdateWeight) {
          FS_LOGD(services::LogComponent::Ui,
                  "Command received request_id=%lu type=%u",
                  static_cast<unsigned long>(command.requestId),
                  static_cast<unsigned>(command.type));
        }
        if (command.type == rtos::UiCommandType::UpdateSpoolPicker &&
            command.value == -1) {
          FS_LOGD(services::LogComponent::Ui,
                  "Stack watermark context=spool_list free_bytes=%u",
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        }
      } while (xQueueReceive(ctx.uiCommandQueue, &command, 0) == pdTRUE);
    }
  }
}
}  // namespace filament_station::tasks
