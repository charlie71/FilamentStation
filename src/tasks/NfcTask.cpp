#include "tasks/Tasks.h"

#include <driver/gpio.h>
#include <driver/uart.h>

#include <cstdio>

#include "config/BoardConfig.h"
#include "config/TaskConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {

namespace {

bool sendNfcError(rtos::RtosContext& ctx, const char* text) {
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::NfcError;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  return xQueueSend(ctx.appEventQueue, &event, 0) == pdPASS;
}

bool initializePn532Uart(QueueHandle_t& uartEventQueue) {
  constexpr uart_port_t uartPort =
      static_cast<uart_port_t>(config::kPn532UartNumber);
  constexpr gpio_num_t txPin =
      static_cast<gpio_num_t>(config::kPn532UartTxPin);
  constexpr gpio_num_t rxPin =
      static_cast<gpio_num_t>(config::kPn532UartRxPin);
  static_assert(GPIO_IS_VALID_OUTPUT_GPIO(txPin),
                "PN532 UART TX must be an output-capable GPIO");
  static_assert(GPIO_IS_VALID_GPIO(rxPin),
                "PN532 UART RX must be a valid GPIO");

  const uart_config_t uartConfig{
      static_cast<int>(config::kPn532UartBaudRate),
      UART_DATA_8_BITS,
      UART_PARITY_DISABLE,
      UART_STOP_BITS_1,
      UART_HW_FLOWCTRL_DISABLE,
      0,
      UART_SCLK_APB,
  };
  if (uart_param_config(uartPort, &uartConfig) != ESP_OK ||
      uart_set_pin(uartPort, txPin, rxPin, UART_PIN_NO_CHANGE,
                   UART_PIN_NO_CHANGE) != ESP_OK) {
    return false;
  }
  return uart_driver_install(
             uartPort, config::kPn532UartRxBufferSize, 0,
             config::kPn532UartEventQueueLength, &uartEventQueue, 0) == ESP_OK;
}

}  // namespace

void nfcTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  QueueHandle_t uartEventQueue = nullptr;
  if (!initializePn532Uart(uartEventQueue)) {
    rtos::logLine("NfcTask: PN532 UART initialization failed");
    sendNfcError(ctx, "PN532 UART initialization failed");
    for (;;) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }

  QueueSetHandle_t inputSet = xQueueCreateSet(
      config::kServiceCommandQueueLength + config::kPn532UartEventQueueLength);
  if (inputSet == nullptr ||
      xQueueAddToSet(ctx.nfcCommandQueue, inputSet) != pdPASS ||
      xQueueAddToSet(uartEventQueue, inputSet) != pdPASS) {
    rtos::logLine("NfcTask: input queue set creation failed");
    sendNfcError(ctx, "PN532 input queue set creation failed");
    for (;;) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }

  rtos::logLine(
      "NfcTask: PN532 UART transport initialized; device not probed");
  rtos::NfcCommand command{};
  uart_event_t uartEvent{};
  for (;;) {
    QueueSetMemberHandle_t ready = xQueueSelectFromSet(inputSet, portMAX_DELAY);
    if (ready == ctx.nfcCommandQueue) {
      xQueueReceive(ctx.nfcCommandQueue, &command, 0);
      // PN532 commands and protocol traffic are implemented in phase 5.3.
    } else if (ready == uartEventQueue &&
               xQueueReceive(uartEventQueue, &uartEvent, 0) == pdPASS) {
      if (uartEvent.type == UART_FIFO_OVF ||
          uartEvent.type == UART_BUFFER_FULL) {
        uart_flush_input(static_cast<uart_port_t>(config::kPn532UartNumber));
        xQueueReset(uartEventQueue);
        sendNfcError(ctx, "PN532 UART receive overflow");
      }
      // UART_DATA is deliberately not parsed before phase 5.3.
    }
  }
}
}  // namespace filament_station::tasks
