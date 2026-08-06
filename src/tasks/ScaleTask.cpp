#include "tasks/Tasks.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_intr_alloc.h>
#include <esp_rom_sys.h>

#include <cstdio>

#include "config/BoardConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/ScaleFilter.h"

namespace filament_station::tasks {

namespace {

TaskHandle_t scaleTaskHandle = nullptr;
portMUX_TYPE hx711ReadMux = portMUX_INITIALIZER_UNLOCKED;

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
  return gpio_isr_handler_add(dataPin, hx711DataReadyIsr, nullptr) == ESP_OK;
}

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

bool sendScaleEvent(rtos::RtosContext& ctx, rtos::AppEventType type,
                    std::int32_t value, const char* text) {
  rtos::AppEvent event{};
  event.type = type;
  event.value = value;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  return xQueueSend(ctx.appEventQueue, &event, 0) == pdPASS;
}

}  // namespace

void scaleTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  scaleTaskHandle = xTaskGetCurrentTaskHandle();
  if (!initializeHx711PinsAndInterrupt()) {
    rtos::logLine("ScaleTask: HX711 GPIO/interrupt initialization failed");
    sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                   "HX711 GPIO/interrupt initialization failed");
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }

  rtos::logLine("ScaleTask: HX711 DOUT interrupt registered");
  services::ScaleFilter filter;
  bool connected = false;
  bool connectionErrorReported = false;
  bool measurementOverflowReported = false;
  for (;;) {
    const std::uint32_t notifications = ulTaskNotifyTake(
        pdTRUE, pdMS_TO_TICKS(config::kHx711ReadyTimeoutMs));
    if (notifications == 0) {
      if (!connectionErrorReported) {
        connected = false;
        connectionErrorReported = true;
        xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_SCALE_READY);
        if (!sendScaleEvent(ctx, rtos::AppEventType::ScaleError, 0,
                            "HX711 not responding")) {
          rtos::logLine("ScaleTask: appEventQueue overflow on ScaleError");
        }
      }
      continue;
    }

    std::int32_t rawCounts = 0;
    if (!readHx711Sample(rawCounts)) continue;
    const std::int32_t filteredCounts = filter.process(rawCounts);

    if (!connected) {
      connected = true;
      connectionErrorReported = false;
      xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_SCALE_READY);
      if (!sendScaleEvent(ctx, rtos::AppEventType::ScaleReady, filteredCounts,
                          "HX711 ready")) {
        rtos::logLine("ScaleTask: appEventQueue overflow on ScaleReady");
      }
    }
    if (!sendScaleEvent(ctx, rtos::AppEventType::ScaleMeasurement,
                        filteredCounts, "HX711 raw measurement")) {
      if (!measurementOverflowReported) {
        measurementOverflowReported = true;
        rtos::logLine("ScaleTask: appEventQueue overflow on ScaleMeasurement");
      }
    } else {
      measurementOverflowReported = false;
    }
  }
}
}  // namespace filament_station::tasks
