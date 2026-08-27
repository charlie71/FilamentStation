/**
 * @file
 * @brief Implements tasks::scaleTask(): bit-banged HX711 driver
 *        (interrupt-driven sample ready detection), filtering, and
 *        calibration command handling.
 */
#include "tasks/Tasks.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_intr_alloc.h>
#include <esp_rom_sys.h>

#include <cstdio>

#include "config/BoardConfig.h"
#include "config/ScaleConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/ScaleFilter.h"
#include "services/ScaleMath.h"
#include "services/Logger.h"

namespace filament_station::tasks {

namespace {

TaskHandle_t scaleTaskHandle = nullptr;  ///< Handle of this task, set at startup so the ISR can notify it.
portMUX_TYPE hx711ReadMux = portMUX_INITIALIZER_UNLOCKED;  ///< Guards the bit-bang critical section in readHx711Sample() against the data-ready ISR.

/// @brief GPIO ISR fired on the HX711 DOUT falling edge; wakes scaleTask().
void IRAM_ATTR hx711DataReadyIsr(void*) {
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  const TaskHandle_t task = scaleTaskHandle;
  if (task != nullptr) {
    vTaskNotifyGiveFromISR(task, &higherPriorityTaskWoken);
  }
  if (higherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

/// @brief Configures the HX711 clock/data GPIOs and installs the data-ready ISR.
/// @return false if any GPIO/ISR configuration step failed.
bool initializeHx711PinsAndInterrupt() {
  constexpr gpio_num_t dataPin =
      static_cast<gpio_num_t>(config::kHx711DataPin);
  constexpr gpio_num_t clockPin =
      static_cast<gpio_num_t>(config::kHx711ClockPin);

  static_assert(GPIO_IS_VALID_GPIO(dataPin),
                "HX711 DOUT must be a valid ESP32-S3 GPIO");
  static_assert(GPIO_IS_VALID_OUTPUT_GPIO(clockPin),
                "HX711 SCK must be an output-capable ESP32-S3 GPIO");

  gpio_config_t clockConfig{};
  clockConfig.pin_bit_mask = 1ULL << config::kHx711ClockPin;
  clockConfig.mode = GPIO_MODE_OUTPUT;
  clockConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  clockConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  clockConfig.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&clockConfig) != ESP_OK || gpio_set_level(clockPin, 0) != ESP_OK) {
    return false;
  }

  gpio_config_t dataConfig{};
  dataConfig.pin_bit_mask = 1ULL << config::kHx711DataPin;
  dataConfig.mode = GPIO_MODE_INPUT;
  // A disconnected DOUT remains high so the timeout can report the fault.
  dataConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  dataConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  dataConfig.intr_type = GPIO_INTR_NEGEDGE;
  if (gpio_config(&dataConfig) != ESP_OK) {
    return false;
  }

  const esp_err_t serviceResult = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (serviceResult != ESP_OK && serviceResult != ESP_ERR_INVALID_STATE) {
    return false;
  }
  if (gpio_isr_handler_add(dataPin, hx711DataReadyIsr, nullptr) != ESP_OK) {
    return false;
  }
  // DOUT may already be low before the edge handler is installed.
  if (gpio_get_level(dataPin) == 0 && scaleTaskHandle != nullptr) {
    xTaskNotifyGive(scaleTaskHandle);
  }
  return true;
}

/// @brief Bit-bangs one 24-bit HX711 conversion result over the clock/data GPIOs.
/// @param rawCounts Out parameter receiving the sign-extended raw reading.
/// @return false if DOUT is not currently low (no sample ready).
bool readHx711Sample(std::int32_t& rawCounts) {
  constexpr gpio_num_t dataPin =
      static_cast<gpio_num_t>(config::kHx711DataPin);
  constexpr gpio_num_t clockPin =
      static_cast<gpio_num_t>(config::kHx711ClockPin);
  if (gpio_get_level(dataPin) != 0) return false;

  std::uint32_t value = 0;
  portENTER_CRITICAL(&hx711ReadMux);
  for (std::uint8_t bit = 0; bit < 24; ++bit) {
    gpio_set_level(clockPin, 1);
    esp_rom_delay_us(1);
    value = (value << 1U) | static_cast<std::uint32_t>(gpio_get_level(dataPin));
    gpio_set_level(clockPin, 0);
    esp_rom_delay_us(1);
  }
  // Pulse 25 selects channel A with gain 128 for the following conversion.
  gpio_set_level(clockPin, 1);
  esp_rom_delay_us(1);
  gpio_set_level(clockPin, 0);
  portEXIT_CRITICAL(&hx711ReadMux);

  if ((value & 0x00800000U) != 0U) value |= 0xFF000000U;
  rawCounts = static_cast<std::int32_t>(value);
  return true;
}

/// @brief Sends a PowerDownAcknowledged command to PowerTask.
/// @param ctx Owning RTOS context.
void sendPowerAck(rtos::RtosContext& ctx) {
  rtos::PowerCommand command{};
  command.type = rtos::PowerCommandType::PowerDownAcknowledged;
  command.source = rtos::PowerPeripheral::Scale;
  if (xQueueSend(ctx.powerCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Scale,
            "Event enqueue failed queue=power_command op=power_down_ack");
  }
}

/// @brief Sends a simple numeric/text AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param value Numeric payload.
/// @param text Text payload.
/// @param requestId Correlation id, defaults to 0 for unsolicited events.
/// @return false if the app event queue was full.
bool sendScaleEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                    std::int32_t value, const char* text,
                    std::uint32_t requestId = 0) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  event.value = value;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  return xQueueSend(ctx.appEventQueue, &event, 0) == pdPASS;
}

/// @brief In-task tare/calibration state, persisted by AppTask across restarts via ApplyCalibration.
struct ScaleCalibrationState {
  std::int32_t offsetCounts = 0;      ///< Raw-counts tare offset.
  float factorCountsPerGram = 1.0F;   ///< Calibration factor.
  bool calibrated = false;            ///< Whether a real calibration has been performed/applied.
};

/// @brief Sends a calibration-state AppEvent to AppTask.
/// @param ctx Owning RTOS context.
/// @param type Event type.
/// @param requestId Correlation id.
/// @param calibration Current calibration state to report.
/// @param text Text payload.
/// @return false if the app event queue was full.
bool sendCalibrationEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                          std::uint32_t requestId,
                          const ScaleCalibrationState& calibration,
                          const char* text) {
  rtos::AppEvent event{};
  event.type = type;
  event.requestId = requestId;
  event.scaleOffsetCounts = calibration.offsetCounts;
  event.scaleFactorCountsPerGram = calibration.factorCountsPerGram;
  event.scaleCalibrated = calibration.calibrated;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  return xQueueSend(ctx.appEventQueue, &event, pdMS_TO_TICKS(100)) == pdPASS;
}

/// @brief Handles one ScaleCommand (tare/calibration/measurement-request), not power up/down.
/// @param ctx Owning RTOS context.
/// @param command Command to process.
/// @param hasMeasurement Whether a valid `latestCounts` reading is currently available.
/// @param latestCounts Most recent filtered raw-counts reading.
/// @param calibration Calibration state to read/update.
/// @param filter Filter to reset on tare/calibration changes.
void processScaleCommand(rtos::RtosContext& ctx,
                         const rtos::ScaleCommand& command,
                         bool hasMeasurement, std::int32_t latestCounts,
                         ScaleCalibrationState& calibration,
                         services::ScaleFilter& filter) {
  switch (command.type) {
    case rtos::ScaleCommandType::Tare:
      if (!hasMeasurement) {
        sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                       "Tare failed: no measurement", command.requestId);
        return;
      }
      calibration.offsetCounts = latestCounts;
      filter.reset();
      sendCalibrationEvent(ctx, rtos::AppEventType::ScaleTared,
                           command.requestId, calibration, "Scale tared");
      return;

    case rtos::ScaleCommandType::StartCalibration: {
      if (!hasMeasurement || command.referenceWeightGrams <= 0.0F) {
        sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                       "Calibration requires measurement and reference weight",
                       command.requestId);
        return;
      }
      float factorCountsPerGram = 0.0F;
      if (!services::calculateScaleFactor(
              latestCounts, calibration.offsetCounts,
              command.referenceWeightGrams, factorCountsPerGram)) {
        sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                       "Calibration values invalid", command.requestId);
        return;
      }
      calibration.factorCountsPerGram = factorCountsPerGram;
      calibration.calibrated = true;
      filter.reset();
      sendCalibrationEvent(ctx, rtos::AppEventType::ScaleCalibrated,
                           command.requestId, calibration,
                           "Scale calibrated");
      return;
    }

    case rtos::ScaleCommandType::ResetCalibration:
      calibration = {};
      filter.reset();
      sendCalibrationEvent(ctx, rtos::AppEventType::ScaleCalibrationReset,
                           command.requestId, calibration,
                           "Scale calibration reset");
      return;

    case rtos::ScaleCommandType::RequestMeasurement:
      if (hasMeasurement) {
        sendScaleEvent(ctx, rtos::AppEventType::ScaleMeasurement,
                       latestCounts, "Scale measurement", command.requestId);
      } else {
        sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                       "Measurement unavailable", command.requestId);
      }
      return;

    case rtos::ScaleCommandType::ApplyCalibration:
      if (command.calibrated && command.factorCountsPerGram == 0.0F) {
        sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                       "Stored calibration factor invalid", command.requestId);
        return;
      }
      calibration.offsetCounts = command.offsetCounts;
      calibration.factorCountsPerGram = command.factorCountsPerGram;
      calibration.calibrated = command.calibrated;
      filter.reset();
      FS_LOGI(services::LogComponent::Scale,
              "Calibration applied calibrated=%s",
              calibration.calibrated ? "true" : "false");
      return;
  }
}

}  // namespace

void scaleTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  scaleTaskHandle = xTaskGetCurrentTaskHandle();
  if (!initializeHx711PinsAndInterrupt()) {
    FS_LOGE(services::LogComponent::Scale,
            "HX711 initialization failed stage=gpio_interrupt");
    sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                   "HX711 GPIO/interrupt initialization failed");
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }

  FS_LOGI(services::LogComponent::Scale,
          "HX711 interrupt registered pin=%d", config::kHx711DataPin);
  const services::ScaleFilterConfig filterConfig{
      config::kScaleMovingAverageWindow,
      config::kScaleLowPassAlpha,
      config::kScaleOutlierThresholdCounts,
      config::kScaleOutlierConfirmationSamples,
      config::kScaleNegativeSmallThresholdCounts,
      config::kScaleStabilityToleranceCounts,
      config::kScaleStabilityTimeMs,
  };
  services::ScaleFilter filter(filterConfig);
  ScaleCalibrationState calibration;
  FS_LOGD(services::LogComponent::Scale,
          "Stack watermark context=initialization free_bytes=%u",
          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  bool connected = false;
  bool connectionErrorReported = false;
  bool measurementOverflowReported = false;
  bool hasMeasurement = false;
  bool poweredDown = false;
  std::int32_t latestCounts = 0;
  std::uint32_t lastMeasurementMs = millis();
  constexpr gpio_num_t clockPin =
      static_cast<gpio_num_t>(config::kHx711ClockPin);
  for (;;) {
    ulTaskNotifyTake(
        pdTRUE, pdMS_TO_TICKS(config::kHx711ReadyTimeoutMs));

    rtos::ScaleCommand command{};
    while (xQueueReceive(ctx.scaleCommandQueue, &command, 0) == pdTRUE) {
      // Energiesparen (TASKS.md Phase 11.3): SCK dauerhaft HIGH haelt das
      // HX711 laut Datenblatt im Power-Down (>60us reichen, hier bleibt es
      // fuer die gesamte Sleep-Dauer stehen). Waehrend dessen werden keine
      // Samples gelesen und der Verbindungs-Timeout unten uebersprungen,
      // damit das keinen "HX711 not responding"-Fehler ausloest.
      if (command.type == rtos::ScaleCommandType::PowerDown) {
        if (!poweredDown) {
          poweredDown = true;
          gpio_set_level(clockPin, 1);
          connected = false;
          hasMeasurement = false;
          connectionErrorReported = false;
          FS_LOGI(services::LogComponent::Scale, "HX711 powered down");
          sendPowerAck(ctx);
        }
        continue;
      }
      if (command.type == rtos::ScaleCommandType::PowerUp) {
        if (poweredDown) {
          poweredDown = false;
          gpio_set_level(clockPin, 0);
          lastMeasurementMs = millis();
          FS_LOGI(services::LogComponent::Scale,
                  "HX711 powered up, waiting for first sample");
        }
        continue;
      }
      processScaleCommand(ctx, command, hasMeasurement, latestCounts,
                          calibration, filter);
    }
    if (poweredDown) continue;

    std::int32_t rawCounts = 0;
    if (readHx711Sample(rawCounts)) {
      const std::uint32_t measurementMs = millis();
      lastMeasurementMs = measurementMs;
      const services::ScaleFilterResult filterResult =
          filter.process(rawCounts, measurementMs);
      const std::int32_t filteredCounts = filterResult.value;
      latestCounts = filteredCounts;
      hasMeasurement = true;

      if (!connected) {
        connected = true;
        connectionErrorReported = false;
        xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SCALE_READY);
        if (!sendScaleEvent(ctx, rtos::AppEventType::ScaleReady, filteredCounts,
                            "HX711 ready")) {
          FS_LOGW(services::LogComponent::Scale,
                  "Event enqueue failed queue=app_event event=ready");
        }
      }
      if (!sendScaleEvent(ctx, rtos::AppEventType::ScaleMeasurement,
                          filteredCounts, "HX711 raw measurement")) {
        if (!measurementOverflowReported) {
          measurementOverflowReported = true;
          FS_LOGW(services::LogComponent::Scale,
                  "Event enqueue failed queue=app_event event=measurement");
        }
      } else {
        measurementOverflowReported = false;
      }
      if (filterResult.stabilityChanged) {
        const rtos::AppEventType eventType =
            filterResult.stable ? rtos::AppEventType::ScaleStable
                                : rtos::AppEventType::ScaleUnstable;
        sendScaleEvent(ctx, eventType, filteredCounts,
                       filterResult.stable ? "Scale stable" : "Scale unstable");
      }
    }

    if (millis() - lastMeasurementMs >= config::kHx711ReadyTimeoutMs &&
        !connectionErrorReported) {
      connected = false;
      connectionErrorReported = true;
      hasMeasurement = false;
      xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SCALE_READY);
      if (!sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                          "HX711 not responding")) {
        FS_LOGW(services::LogComponent::Scale,
                "Event enqueue failed queue=app_event event=error");
      }
    }
  }
}
}  // namespace filament_station::tasks
