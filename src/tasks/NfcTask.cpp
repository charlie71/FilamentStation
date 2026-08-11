#include "tasks/Tasks.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "config/BoardConfig.h"
#include "config/NfcConfig.h"
#include "config/TaskConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/NfcPayload.h"

namespace filament_station::tasks {
namespace {

constexpr uart_port_t kUart =
    static_cast<uart_port_t>(config::kPn532UartNumber);
constexpr std::uint8_t kPn532HostToDevice = 0xD4;
constexpr std::uint8_t kPn532DeviceToHost = 0xD5;
constexpr std::uint8_t kCommandGetFirmwareVersion = 0x02;
constexpr std::uint8_t kCommandSamConfiguration = 0x14;
constexpr std::uint8_t kCommandRfConfiguration = 0x32;
constexpr std::uint8_t kCommandInDataExchange = 0x40;
constexpr std::uint8_t kCommandInListPassiveTarget = 0x4A;
constexpr std::uint8_t kMifareRead = 0x30;
constexpr std::uint8_t kMifareWrite = 0xA2;
std::size_t lastTransactionRxBytes = 0;

struct TargetInfo {
  std::array<std::uint8_t, config::kNfcMaxUidLength> uid{};
  std::uint8_t uidLength = 0;
  std::uint8_t targetNumber = 0;
  std::uint8_t sak = 0;
};

enum class Pn532InitializationResult : std::uint8_t {
  Ready,
  FirmwareVersionFailed,
  SamConfigurationFailed,
  RfConfigurationFailed,
};

const char* initializationStep(Pn532InitializationResult result) {
  switch (result) {
    case Pn532InitializationResult::FirmwareVersionFailed:
      return "GetFirmwareVersion";
    case Pn532InitializationResult::SamConfigurationFailed:
      return "SAMConfiguration";
    case Pn532InitializationResult::RfConfigurationFailed:
      return "RFConfiguration";
    case Pn532InitializationResult::Ready:
      return "ready";
  }
  return "unknown";
}

bool sendEvent(rtos::RtosContext& ctx, rtos::AppEvent event) {
  if (xQueueSend(ctx.appEventQueue, &event, 0) == pdPASS) return true;
  rtos::logLine("NfcTask: app event queue full");
  return false;
}

void sendError(rtos::RtosContext& ctx, std::uint32_t requestId,
               const char* text) {
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::NfcError;
  event.requestId = requestId;
  std::snprintf(event.text, sizeof(event.text), "%s", text);
  sendEvent(ctx, event);
}

bool initializeUart() {
  constexpr gpio_num_t txPin =
      static_cast<gpio_num_t>(config::kPn532UartTxPin);
  constexpr gpio_num_t rxPin =
      static_cast<gpio_num_t>(config::kPn532UartRxPin);
  static_assert(GPIO_IS_VALID_OUTPUT_GPIO(txPin));
  static_assert(GPIO_IS_VALID_GPIO(rxPin));
  static_assert(txPin != rxPin);
  static_assert(config::kPn532UartTxPin != config::kTouchSdaPin &&
                    config::kPn532UartTxPin != config::kTouchSclPin &&
                    config::kPn532UartRxPin != config::kTouchSdaPin &&
                    config::kPn532UartRxPin != config::kTouchSclPin,
                "PN532 UART must not use touch I2C pins");
  static_assert(config::kPn532UartTxPin != config::kHx711DataPin &&
                    config::kPn532UartTxPin != config::kHx711ClockPin &&
                    config::kPn532UartRxPin != config::kHx711DataPin &&
                    config::kPn532UartRxPin != config::kHx711ClockPin,
                "PN532 UART must not use HX711 pins");
  const uart_config_t uartConfig{
      static_cast<int>(config::kPn532UartBaudRate), UART_DATA_8_BITS,
      UART_PARITY_DISABLE, UART_STOP_BITS_1, UART_HW_FLOWCTRL_DISABLE, 0,
      UART_SCLK_APB};
  if (uart_param_config(kUart, &uartConfig) != ESP_OK ||
      uart_set_pin(kUart, txPin, rxPin, UART_PIN_NO_CHANGE,
                   UART_PIN_NO_CHANGE) != ESP_OK) {
    return false;
  }
  return uart_driver_install(kUart, config::kPn532UartRxBufferSize, 0, 0,
                             nullptr, 0) == ESP_OK;
}

bool readExact(std::uint8_t* output, std::size_t length,
               std::uint32_t timeoutMs) {
  const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
  std::size_t received = 0;
  while (received < length) {
    const TickType_t now = xTaskGetTickCount();
    if (static_cast<std::int32_t>(deadline - now) <= 0) return false;
    const int count = uart_read_bytes(kUart, output + received,
                                      length - received, deadline - now);
    if (count <= 0) return false;
    received += static_cast<std::size_t>(count);
    lastTransactionRxBytes += static_cast<std::size_t>(count);
  }
  return true;
}

bool findFrameStart(std::uint32_t timeoutMs) {
  const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
  std::uint8_t state = 0;
  while (static_cast<std::int32_t>(deadline - xTaskGetTickCount()) > 0) {
    std::uint8_t byte = 0;
    if (!readExact(&byte, 1, pdTICKS_TO_MS(deadline - xTaskGetTickCount()))) {
      return false;
    }
    if ((state == 0 && byte == 0x00) || (state == 1 && byte == 0x00)) {
      ++state;
    } else if (state == 2 && byte == 0xFF) {
      return true;
    } else {
      state = byte == 0x00 ? 1 : 0;
    }
  }
  return false;
}

bool receiveFrame(std::uint8_t* output, std::size_t capacity,
                  std::size_t& outputLength) {
  outputLength = 0;
  const std::uint32_t timeout = config::kPn532ResponseTimeoutMs;
  if (!findFrameStart(timeout)) return false;
  std::uint8_t header[2]{};
  if (!readExact(header, sizeof(header), timeout)) return false;
  if (header[0] == 0x00 && header[1] == 0xFF) {
    std::uint8_t postamble = 0;
    if (!readExact(&postamble, 1, timeout) || postamble != 0x00 ||
        !findFrameStart(timeout) ||
        !readExact(header, sizeof(header), timeout)) {
      return false;
    }
  }
  const std::uint8_t length = header[0];
  if (static_cast<std::uint8_t>(length + header[1]) != 0 || length < 2 ||
      length > capacity + 1) {
    return false;
  }
  std::array<std::uint8_t, 258> frame{};
  if (!readExact(frame.data(), static_cast<std::size_t>(length) + 2U,
                 timeout)) {
    return false;
  }
  std::uint8_t checksum = 0;
  for (std::size_t i = 0; i < length; ++i) checksum += frame[i];
  // PN532-DCS ist eine 8-Bit-Zweierkomplement-Pruefsumme. Durch die uebliche
  // Integer-Promotion muss die Addition explizit wieder auf 8 Bit begrenzt
  // werden; eine gueltige Summe ergibt als int sonst 256 statt 0.
  if (static_cast<std::uint8_t>(checksum + frame[length]) != 0 ||
      frame[length + 1] != 0x00 ||
      frame[0] != kPn532DeviceToHost) {
    return false;
  }
  outputLength = length - 1U;
  std::memcpy(output, frame.data() + 1, outputLength);
  return true;
}

bool transceive(std::uint8_t command, const std::uint8_t* data,
                std::size_t dataLength, std::uint8_t* response,
                std::size_t responseCapacity, std::size_t& responseLength) {
  if (dataLength > 252) return false;
  lastTransactionRxBytes = 0;
  std::array<std::uint8_t, 264> frame{};
  const std::uint8_t length = static_cast<std::uint8_t>(dataLength + 2U);
  std::size_t p = 0;
  frame[p++] = 0x00;
  frame[p++] = 0x00;
  frame[p++] = 0xFF;
  frame[p++] = length;
  frame[p++] = static_cast<std::uint8_t>(0U - length);
  frame[p++] = kPn532HostToDevice;
  frame[p++] = command;
  std::uint8_t checksum = kPn532HostToDevice + command;
  for (std::size_t i = 0; i < dataLength; ++i) {
    frame[p++] = data[i];
    checksum += data[i];
  }
  frame[p++] = static_cast<std::uint8_t>(0U - checksum);
  frame[p++] = 0x00;
  uart_flush_input(kUart);
  if (uart_write_bytes(kUart, frame.data(), p) != static_cast<int>(p) ||
      uart_wait_tx_done(kUart, pdMS_TO_TICKS(100)) != ESP_OK ||
      !receiveFrame(response, responseCapacity, responseLength)) {
    return false;
  }
  return responseLength >= 1 && response[0] == command + 1U;
}

Pn532InitializationResult initializePn532() {
  const std::uint8_t wake[] = {0x55, 0x55, 0x00, 0x00, 0x00};
  uart_write_bytes(kUart, wake, sizeof(wake));
  uart_wait_tx_done(kUart, pdMS_TO_TICKS(100));
  std::uint8_t response[32]{};
  std::size_t length = 0;
  if (!transceive(kCommandGetFirmwareVersion, nullptr, 0, response,
                  sizeof(response), length) ||
      length < 5) {
    return Pn532InitializationResult::FirmwareVersionFailed;
  }
  {
    char firmware[112]{};
    std::snprintf(firmware, sizeof(firmware),
                  "NfcTask: PN532 firmware response: IC=0x%02X, version=%u.%u, "
                  "support=0x%02X",
                  response[1], response[2], response[3], response[4]);
    rtos::logLine(firmware);
  }
  const std::uint8_t sam[] = {0x01, 0x14, 0x01};
  if (!transceive(kCommandSamConfiguration, sam, sizeof(sam), response,
                  sizeof(response), length)) {
    return Pn532InitializationResult::SamConfigurationFailed;
  }
  rtos::logLine("NfcTask: PN532 SAM configured");
  const std::uint8_t retries[] = {0x05, 0xFF, 0x01,
                                  config::kPn532MaxTargetRetries};
  if (!transceive(kCommandRfConfiguration, retries, sizeof(retries), response,
                  sizeof(response), length)) {
    return Pn532InitializationResult::RfConfigurationFailed;
  }
  rtos::logLine("NfcTask: PN532 RF configured");
  return Pn532InitializationResult::Ready;
}

bool scanTarget(TargetInfo& target) {
  const std::uint8_t params[] = {0x01, 0x00};
  std::uint8_t response[32]{};
  std::size_t length = 0;
  if (!transceive(kCommandInListPassiveTarget, params, sizeof(params), response,
                  sizeof(response), length) ||
      length < 2 || response[1] == 0) {
    return false;
  }
  if (length < 7) return false;
  const std::uint8_t uidLength = response[6];
  if (uidLength == 0 || uidLength > target.uid.size() ||
      7U + uidLength > length) {
    return false;
  }
  target = {};
  target.targetNumber = response[2];
  target.sak = response[5];
  target.uidLength = uidLength;
  std::memcpy(target.uid.data(), response + 7, uidLength);
  return true;
}

bool exchange(const TargetInfo& target, const std::uint8_t* command,
              std::size_t commandLength, std::uint8_t* response,
              std::size_t capacity, std::size_t& length) {
  std::array<std::uint8_t, 16> params{};
  if (commandLength + 1 > params.size()) return false;
  params[0] = target.targetNumber;
  std::memcpy(params.data() + 1, command, commandLength);
  std::array<std::uint8_t, 32> raw{};
  std::size_t rawLength = 0;
  if (!transceive(kCommandInDataExchange, params.data(), commandLength + 1,
                  raw.data(), raw.size(), rawLength) ||
      rawLength < 2 || (raw[1] & 0x3FU) != 0 || rawLength - 2 > capacity) {
    return false;
  }
  length = rawLength - 2;
  std::memcpy(response, raw.data() + 2, length);
  return true;
}

bool readPages(const TargetInfo& target, std::uint8_t firstPage,
               std::uint8_t* output) {
  const std::uint8_t command[] = {kMifareRead, firstPage};
  std::size_t length = 0;
  return exchange(target, command, sizeof(command), output, 16, length) &&
         length >= 16;
}

bool writePage(const TargetInfo& target, std::uint8_t page,
               const std::uint8_t* bytes) {
  const std::uint8_t command[] = {kMifareWrite, page, bytes[0], bytes[1],
                                  bytes[2], bytes[3]};
  std::uint8_t response[4]{};
  std::size_t length = 0;
  return exchange(target, command, sizeof(command), response, sizeof(response),
                  length);
}

bool readNdef(const TargetInfo& target, std::uint8_t* output,
              std::size_t capacity, std::size_t& length) {
  std::uint8_t first[16]{};
  if (!readPages(target, 3, first) || first[0] != 0xE1) return false;
  const std::size_t tagCapacity = static_cast<std::size_t>(first[2]) * 8U;
  length = std::min(tagCapacity, capacity);
  std::size_t copied = 0;
  std::uint8_t page = 4;
  while (copied < length) {
    std::uint8_t block[16]{};
    if (!readPages(target, page, block)) return false;
    const std::size_t count = std::min<std::size_t>(16, length - copied);
    std::memcpy(output + copied, block, count);
    copied += count;
    page = static_cast<std::uint8_t>(page + 4U);
  }
  return true;
}

bool writeNdef(const TargetInfo& target, const std::uint8_t* bytes,
               std::size_t length) {
  if (length == 0 || (length % 4) != 0) return false;
  std::uint8_t capability[16]{};
  if (!readPages(target, 3, capability) || capability[0] != 0xE1 ||
      (capability[3] & 0x0FU) == 0x0FU ||
      length > static_cast<std::size_t>(capability[2]) * 8U) {
    return false;
  }
  for (std::size_t offset = 0; offset < length; offset += 4) {
    if (!writePage(target, static_cast<std::uint8_t>(4U + offset / 4U),
                   bytes + offset)) {
      return false;
    }
  }
  return true;
}

rtos::NfcTagType convertType(services::NfcPayloadType type) {
  switch (type) {
    case services::NfcPayloadType::Spoolman:
      return rtos::NfcTagType::Spoolman;
    case services::NfcPayloadType::Bambu:
      return rtos::NfcTagType::Bambu;
    case services::NfcPayloadType::Legacy:
      return rtos::NfcTagType::Legacy;
    default:
      return rtos::NfcTagType::Unknown;
  }
}

void fillTarget(rtos::AppEvent& event, const TargetInfo& target) {
  event.nfcUidLength = target.uidLength;
  std::memcpy(event.nfcUid, target.uid.data(), target.uidLength);
}

void reportTag(rtos::RtosContext& ctx, const TargetInfo& target) {
  rtos::AppEvent detected{};
  detected.type = rtos::AppEventType::NfcTagDetected;
  fillTarget(detected, target);
  sendEvent(ctx, detected);

  rtos::AppEvent read{};
  read.type = rtos::AppEventType::NfcTagRead;
  fillTarget(read, target);
  // MIFARE Classic SAK values are treated as a Bambu candidate. Actual Bambu
  // block decoding belongs to phase 5.5 and is intentionally not attempted.
  if (target.sak == 0x08 || target.sak == 0x18) {
    read.nfcTagType = rtos::NfcTagType::Bambu;
    std::snprintf(read.text, sizeof(read.text), "Bambu-compatible tag detected");
    sendEvent(ctx, read);
    return;
  }
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> ndef{};
  std::size_t length = 0;
  if (!readNdef(target, ndef.data(), ndef.size(), length)) {
    read.nfcTagType = rtos::NfcTagType::Unknown;
    std::snprintf(read.text, sizeof(read.text), "Tag has no readable Type-2 NDEF");
  } else {
    const auto info = services::parseType2Ndef(ndef.data(), length);
    read.nfcTagType = convertType(info.type);
    read.spoolId = info.spoolId;
    std::snprintf(read.text, sizeof(read.text), "NFC tag read");
  }
  sendEvent(ctx, read);
}

bool sameUid(const TargetInfo& left, const TargetInfo& right) {
  return left.uidLength == right.uidLength &&
         std::memcmp(left.uid.data(), right.uid.data(), left.uidLength) == 0;
}

void handleWrite(rtos::RtosContext& ctx, const rtos::NfcCommand& command,
                 const TargetInfo& target) {
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> bytes{};
  std::size_t length = 0;
  if (!services::buildSpoolmanType2Ndef(command.spoolId, bytes.data(),
                                        bytes.size(), length) ||
      !writeNdef(target, bytes.data(), length)) {
    sendError(ctx, command.requestId, "NFC tag write failed");
    return;
  }
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> verify{};
  std::size_t verifyLength = 0;
  const auto result =
      readNdef(target, verify.data(), verify.size(), verifyLength)
          ? services::parseType2Ndef(verify.data(), verifyLength)
          : services::NfcPayloadInfo{};
  if (result.type != services::NfcPayloadType::Spoolman ||
      result.spoolId != command.spoolId) {
    sendError(ctx, command.requestId, "NFC tag verification failed");
    return;
  }
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::NfcTagWritten;
  event.requestId = command.requestId;
  event.spoolId = command.spoolId;
  event.nfcTagType = rtos::NfcTagType::Spoolman;
  fillTarget(event, target);
  std::snprintf(event.text, sizeof(event.text), "NFC tag written and verified");
  sendEvent(ctx, event);
}

void handleErase(rtos::RtosContext& ctx, const rtos::NfcCommand& command,
                 const TargetInfo& target) {
  const std::uint8_t empty[] = {0x03, 0x00, 0xFE, 0x00};
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> verify{};
  std::size_t length = 0;
  if (!writeNdef(target, empty, sizeof(empty)) ||
      !readNdef(target, verify.data(), verify.size(), length) ||
      services::parseType2Ndef(verify.data(), length).type !=
          services::NfcPayloadType::Empty) {
    sendError(ctx, command.requestId, "NFC tag erase verification failed");
    return;
  }
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::NfcTagErased;
  event.requestId = command.requestId;
  fillTarget(event, target);
  std::snprintf(event.text, sizeof(event.text), "NFC tag erased and verified");
  sendEvent(ctx, event);
}

}  // namespace

void nfcTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  if (!initializeUart()) {
    rtos::logLine("NfcTask: PN532 UART initialization failed");
    sendError(ctx, 0, "PN532 UART initialization failed");
    for (;;) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
  {
    char uartStatus[112]{};
    std::snprintf(uartStatus, sizeof(uartStatus),
                  "NfcTask: UART%d ready, TX=GPIO%d -> SCL/RXD, "
                  "RX=GPIO%d <- SDA/TXD, %lu 8N1",
                  config::kPn532UartNumber, config::kPn532UartTxPin,
                  config::kPn532UartRxPin,
                  static_cast<unsigned long>(config::kPn532UartBaudRate));
    rtos::logLine(uartStatus);
  }
  const Pn532InitializationResult initializationResult = initializePn532();
  if (initializationResult != Pn532InitializationResult::Ready) {
    char initializationError[128]{};
    if (lastTransactionRxBytes == 0) {
      std::snprintf(initializationError, sizeof(initializationError),
                    "NfcTask: PN532 %s failed; no UART bytes received",
                    initializationStep(initializationResult));
      rtos::logLine(initializationError);
      sendError(ctx, 0, "PN532 not responding; check HSU wiring");
    } else {
      std::snprintf(initializationError, sizeof(initializationError),
                    "NfcTask: PN532 %s invalid/incomplete (%u UART bytes)",
                    initializationStep(initializationResult),
                    static_cast<unsigned>(lastTransactionRxBytes));
      rtos::logLine(initializationError);
      sendError(ctx, 0, "PN532 returned an invalid UART frame");
    }
  } else {
    rtos::logLine("NfcTask: PN532 ready");
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_NFC_READY);
    rtos::AppEvent event{};
    event.type = rtos::AppEventType::NfcInitialized;
    std::snprintf(event.text, sizeof(event.text), "PN532 ready");
    sendEvent(ctx, event);
  }

  bool reading = true;
  bool present = false;
  TargetInfo active{};
  std::uint32_t lastSeenMs = 0;
  rtos::NfcCommand command{};
  for (;;) {
    if (xQueueReceive(ctx.nfcCommandQueue, &command,
                      pdMS_TO_TICKS(config::kNfcScanIntervalMs)) == pdPASS) {
      switch (command.type) {
        case rtos::NfcCommandType::StartReading:
          reading = true;
          break;
        case rtos::NfcCommandType::StopReading:
          reading = false;
          break;
        case rtos::NfcCommandType::WriteSpoolTag:
          if (present)
            handleWrite(ctx, command, active);
          else
            sendError(ctx, command.requestId, "No NFC tag present");
          break;
        case rtos::NfcCommandType::EraseTag:
          if (present)
            handleErase(ctx, command, active);
          else
            sendError(ctx, command.requestId, "No NFC tag present");
          break;
      }
      continue;
    }
    if (!reading) continue;
    TargetInfo found{};
    if (scanTarget(found)) {
      lastSeenMs = millis();
      if (!present || !sameUid(active, found)) {
        active = found;
        present = true;
        reportTag(ctx, active);
      }
    } else if (present && millis() - lastSeenMs >= config::kNfcTagRemovalDelayMs) {
      rtos::AppEvent event{};
      event.type = rtos::AppEventType::NfcTagRemoved;
      fillTarget(event, active);
      std::snprintf(event.text, sizeof(event.text), "NFC tag removed");
      sendEvent(ctx, event);
      present = false;
      active = {};
    }
  }
}

}  // namespace filament_station::tasks
