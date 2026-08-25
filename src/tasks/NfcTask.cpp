#include "tasks/Tasks.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <mbedtls/md.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "config/BoardConfig.h"
#include "config/NfcConfig.h"
#include "config/TaskConfig.h"
#include "models/TagReadResult.h"
#include "nfc/TagParserRegistry.h"
#include "nfc/TagWritePolicy.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"
#include "services/NfcPayload.h"
#include "services/Ntag21x.h"
#include "services/PsramAlloc.h"

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
constexpr std::uint8_t kCommandInCommunicateThru = 0x42;
constexpr std::uint8_t kCommandInListPassiveTarget = 0x4A;
constexpr std::uint8_t kCommandInRelease = 0x52;
// Energiesparen (TASKS.md Phase 11.4). WakeUpEnable-Bitmap laut PN532-
// Datenblatt (UM0701-02, 7.2.11 PowerDown): je ein Bit pro Interface, Bit4 =
// HSU -- exakt das hier verwendete Transportinterface. Wake erfolgt danach
// wie gewohnt ueber die bereits vorhandene Wake-Praeambel in
// initializePn532() (UART-RX-Aktivitaet weckt den Chip).
constexpr std::uint8_t kCommandPowerDown = 0x16;
constexpr std::uint8_t kPn532WakeUpEnableHsu = 0x10;
constexpr std::uint8_t kMifareRead = 0x30;
constexpr std::uint8_t kMifareAuthenticateKeyA = 0x60;
constexpr std::uint8_t kMifareWrite = 0xA2;
constexpr std::uint8_t kGetVersion = 0x60;
std::size_t lastTransactionRxBytes = 0;

struct TargetInfo {
  std::array<std::uint8_t, config::kNfcMaxUidLength> uid{};
  std::uint8_t uidLength = 0;
  std::uint8_t targetNumber = 0;
  std::uint8_t sak = 0;
};

enum class ScanResult : std::uint8_t {
  Found,
  NoTarget,
  CommunicationError,
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

// Takes AppEvent by reference, not by value: AppEvent has grown considerably
// across the Bambu phases (PrinterState, BambuConfigCollection, per-slot
// material/color), and every by-value call site briefly doubled its stack
// footprint (caller's local plus the parameter copy) on top of an already
// stack-heavy call chain (MIFARE auth, HKDF-SHA256, tag parsing) -- a real
// contributor to the stack-overflow crashes this task has hit.
bool sendEvent(rtos::RtosContext& ctx, const rtos::AppEvent& event) {
  if (xQueueSend(ctx.appEventQueue, &event, 0) == pdPASS) return true;
  FS_LOGW(services::LogComponent::Nfc,
          "Event enqueue failed queue=app_event event=%u",
          static_cast<unsigned>(event.type));
  return false;
}

void sendPowerAck(rtos::RtosContext& ctx) {
  rtos::PowerCommand command{};
  command.type = rtos::PowerCommandType::PowerDownAcknowledged;
  command.source = rtos::PowerPeripheral::Nfc;
  if (xQueueSend(ctx.powerCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Nfc,
            "Event enqueue failed queue=power_command op=power_down_ack");
  }
}

void sendError(rtos::RtosContext& ctx, std::uint32_t requestId,
               const char* text) {
  // static, PSRAM-backed (services/PsramAlloc.h): see the AppEvent size
  // note on reportTag()'s `detected`/`read` below -- the same reasoning
  // applies to every AppEvent this task builds.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>("NfcTask.sendError");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::NfcError;
  event->requestId = requestId;
  std::snprintf(event->text, sizeof(event->text), "%s", text);
  sendEvent(ctx, *event);
}

// PN532 runtime disconnect/reconnect (Robustheit/Diagnose, TASKS.md 10.2):
// unlike boot-time initializePn532() failures, a wire coming loose mid-
// session previously went completely unnoticed -- scanTarget() communication
// errors only ever triggered a silent resetRfField() retry, with no upper
// bound and no notification to AppTask/UI, and a tag that happened to be
// selected at the time would eventually be reported as physically "removed"
// instead. sustainedCommErrors persists across those soft resets (unlike
// consecutiveScanErrors) so a genuinely dead reader is still caught even
// though it keeps getting "recovery" attempts.
void reportPn532Disconnected(rtos::RtosContext& ctx) {
  xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_NFC_READY);
  FS_LOGE(services::LogComponent::Nfc,
          "PN532 not responding at runtime; treating as disconnected");
  sendError(ctx, 0, "PN532 not responding; check HSU wiring");
}

void reportPn532Reconnected(rtos::RtosContext& ctx) {
  xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_NFC_READY);
  FS_LOGI(services::LogComponent::Nfc, "PN532 responding again");
  // static, PSRAM-backed: see sendError() above / services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "NfcTask.reportPn532Reconnected");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::NfcInitialized;
  std::snprintf(event->text, sizeof(event->text), "PN532 ready");
  sendEvent(ctx, *event);
}

void notePn532Responding(rtos::RtosContext& ctx, bool& pn532Connected,
                         std::uint16_t& sustainedCommErrors) {
  sustainedCommErrors = 0;
  if (!pn532Connected) {
    pn532Connected = true;
    reportPn532Reconnected(ctx);
  }
}

void notePn532CommError(rtos::RtosContext& ctx, bool& pn532Connected,
                        std::uint16_t& sustainedCommErrors) {
  if (sustainedCommErrors < config::kPn532DisconnectConfirmationScans) {
    ++sustainedCommErrors;
  }
  if (pn532Connected &&
      sustainedCommErrors >= config::kPn532DisconnectConfirmationScans) {
    pn532Connected = false;
    reportPn532Disconnected(ctx);
  }
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
    if (!readExact(&byte, 1, pdTICKS_TO_MS(deadline - xTaskGetTickCount())))
      return false;
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
        !readExact(header, sizeof(header), timeout))
      return false;
  }
  const std::uint8_t length = header[0];
  if (static_cast<std::uint8_t>(length + header[1]) != 0 || length < 2 ||
      length > capacity + 1)
    return false;
  std::array<std::uint8_t, 258> frame{};
  if (!readExact(frame.data(), static_cast<std::size_t>(length) + 2U, timeout))
    return false;
  std::uint8_t checksum = 0;
  for (std::size_t index = 0; index < length; ++index) checksum += frame[index];
  if (static_cast<std::uint8_t>(checksum + frame[length]) != 0 ||
      frame[length + 1U] != 0x00 || frame[0] != kPn532DeviceToHost)
    return false;
  outputLength = length - 1U;
  std::memcpy(output, frame.data() + 1U, outputLength);
  return true;
}

bool transceive(std::uint8_t command, const std::uint8_t* data,
                std::size_t dataLength, std::uint8_t* response,
                std::size_t responseCapacity, std::size_t& responseLength) {
  if (dataLength > 252) return false;
  lastTransactionRxBytes = 0;
  std::array<std::uint8_t, 264> frame{};
  const std::uint8_t length = static_cast<std::uint8_t>(dataLength + 2U);
  std::size_t position = 0;
  frame[position++] = 0x00;
  frame[position++] = 0x00;
  frame[position++] = 0xFF;
  frame[position++] = length;
  frame[position++] = static_cast<std::uint8_t>(0U - length);
  frame[position++] = kPn532HostToDevice;
  frame[position++] = command;
  std::uint8_t checksum = static_cast<std::uint8_t>(kPn532HostToDevice + command);
  for (std::size_t index = 0; index < dataLength; ++index) {
    frame[position++] = data[index];
    checksum = static_cast<std::uint8_t>(checksum + data[index]);
  }
  frame[position++] = static_cast<std::uint8_t>(0U - checksum);
  frame[position++] = 0x00;
  uart_flush_input(kUart);
  if (uart_write_bytes(kUart, frame.data(), position) !=
          static_cast<int>(position) ||
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
  FS_LOGI(services::LogComponent::Nfc,
          "PN532 firmware ic=0x%02X version=%u.%u support=0x%02X",
          response[1], response[2], response[3], response[4]);
  const std::uint8_t sam[] = {0x01, 0x14, 0x01};
  if (!transceive(kCommandSamConfiguration, sam, sizeof(sam), response,
                  sizeof(response), length)) {
    return Pn532InitializationResult::SamConfigurationFailed;
  }
  FS_LOGI(services::LogComponent::Nfc, "PN532 SAM configured");
  const std::uint8_t retries[] = {0x05, 0xFF, 0x01,
                                  config::kPn532MaxTargetRetries};
  if (!transceive(kCommandRfConfiguration, retries, sizeof(retries), response,
                  sizeof(response), length)) {
    return Pn532InitializationResult::RfConfigurationFailed;
  }
  FS_LOGI(services::LogComponent::Nfc, "PN532 RF configured");
  return Pn532InitializationResult::Ready;
}

ScanResult scanTarget(TargetInfo& target) {
  // Activate at most one passive ISO14443A target at 106 kbit/s.
  const std::uint8_t params[] = {0x01, 0x00};
  std::uint8_t response[32]{};
  std::size_t length = 0;
  if (!transceive(kCommandInListPassiveTarget, params, sizeof(params), response,
                  sizeof(response), length) || length < 2) {
    return ScanResult::CommunicationError;
  }
  if (response[1] == 0) return ScanResult::NoTarget;
  // D5 4B NbTg Tg SENS_RES[2] SEL_RES UIDLen UID...
  if (length < 7) return ScanResult::CommunicationError;
  const std::uint8_t uidLength = response[6];
  if (uidLength == 0 || uidLength > target.uid.size() ||
      7U + uidLength > length) {
    return ScanResult::CommunicationError;
  }
  target = {};
  target.targetNumber = response[2];
  target.sak = response[5];
  target.uidLength = uidLength;
  std::memcpy(target.uid.data(), response + 7, uidLength);
  return ScanResult::Found;
}

bool setRfField(bool enabled) {
  const std::uint8_t params[] = {0x01,
                                 static_cast<std::uint8_t>(enabled ? 0x01 : 0x00)};
  std::uint8_t response[4]{};
  std::size_t length = 0;
  return transceive(kCommandRfConfiguration, params, sizeof(params), response,
                    sizeof(response), length);
}

bool powerDown() {
  const std::uint8_t params[] = {kPn532WakeUpEnableHsu};
  std::uint8_t response[4]{};
  std::size_t length = 0;
  return transceive(kCommandPowerDown, params, sizeof(params), response,
                    sizeof(response), length);
}

void resetRfField() {
  const std::uint8_t releaseAll[] = {0x00};
  std::uint8_t response[4]{};
  std::size_t responseLength = 0;
  transceive(kCommandInRelease, releaseAll, sizeof(releaseAll), response,
             sizeof(response), responseLength);
  setRfField(false);
  vTaskDelay(pdMS_TO_TICKS(20));
  setRfField(true);
  vTaskDelay(pdMS_TO_TICKS(10));
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

bool communicateThru(const std::uint8_t* command, std::size_t commandLength,
                     std::uint8_t* response, std::size_t capacity,
                     std::size_t& length) {
  std::array<std::uint8_t, 32> raw{};
  std::size_t rawLength = 0;
  if (!transceive(kCommandInCommunicateThru, command, commandLength,
                  raw.data(), raw.size(), rawLength) ||
      rawLength < 2 || (raw[1] & 0x3FU) != 0 ||
      rawLength - 2U > capacity) {
    return false;
  }
  length = rawLength - 2U;
  std::memcpy(response, raw.data() + 2, length);
  return true;
}

bool readPages(const TargetInfo& target, std::uint8_t firstPage,
               std::uint8_t* output) {
  const std::uint8_t command[] = {kMifareRead, firstPage};
  for (std::uint8_t attempt = 0; attempt < 3; ++attempt) {
    std::size_t length = 0;
    if (exchange(target, command, sizeof(command), output, 16, length) &&
        length >= 16)
      return true;
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  return false;
}

bool deriveBambuKeys(const TargetInfo& target,
                     std::array<std::uint8_t, 16 * 6>& keys) {
  if (target.uidLength != 4) return false;
  constexpr std::uint8_t salt[] = {0x9A, 0x75, 0x9C, 0xF2, 0xC4, 0xF7,
                                   0xCA, 0xFF, 0x22, 0x2C, 0xB9, 0x76,
                                   0x9B, 0x41, 0xBC, 0x96};
  constexpr std::uint8_t info[] = {'R', 'F', 'I', 'D', '-', 'A', 0x00};
  const mbedtls_md_info_t* sha256 =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (sha256 == nullptr) return false;

  // HKDF-SHA256 nach RFC 5869. Arduino-ESP32 exportiert in der verwendeten
  // mbedTLS-Konfiguration nicht mbedtls_hkdf(), wohl aber den HMAC-Baustein.
  std::array<std::uint8_t, 32> prk{};
  if (mbedtls_md_hmac(sha256, salt, sizeof(salt), target.uid.data(),
                      target.uidLength, prk.data()) != 0) {
    return false;
  }

  std::array<std::uint8_t, 32> previous{};
  std::array<std::uint8_t, 32 + sizeof(info) + 1> input{};
  std::size_t generated = 0;
  std::size_t previousLength = 0;
  std::uint8_t counter = 1;
  while (generated < keys.size()) {
    std::memcpy(input.data(), previous.data(), previousLength);
    std::memcpy(input.data() + previousLength, info, sizeof(info));
    input[previousLength + sizeof(info)] = counter;
    if (mbedtls_md_hmac(sha256, prk.data(), prk.size(), input.data(),
                        previousLength + sizeof(info) + 1,
                        previous.data()) != 0) {
      return false;
    }
    const std::size_t count =
        std::min(previous.size(), keys.size() - generated);
    std::memcpy(keys.data() + generated, previous.data(), count);
    generated += count;
    previousLength = previous.size();
    ++counter;
  }
  return true;
}

bool authenticateMifareBlock(const TargetInfo& target, std::uint8_t block,
                             const std::uint8_t* key) {
  if (target.uidLength != 4) return false;
  std::uint8_t command[12]{kMifareAuthenticateKeyA, block};
  std::memcpy(command + 2, key, 6);
  std::memcpy(command + 8, target.uid.data(), 4);
  std::uint8_t response[4]{};
  std::size_t length = 0;
  return exchange(target, command, sizeof(command), response,
                  sizeof(response), length);
}

bool readMifareBlock(const TargetInfo& target, std::uint8_t block,
                     std::uint8_t* output) {
  const std::uint8_t command[] = {kMifareRead, block};
  std::size_t length = 0;
  return exchange(target, command, sizeof(command), output, 16, length) &&
         length >= 16;
}

void readBambuBlocks(const TargetInfo& target, models::RawTagData& raw) {
  if (raw.technology != models::TagTechnology::MifareClassic1K) return;
  std::array<std::uint8_t, 16 * 6> keys{};
  if (!deriveBambuKeys(target, keys)) return;
  constexpr std::array<std::uint8_t, 7> blocks{{1, 2, 4, 5, 6, 9, 16}};
  std::int8_t authenticatedSector = -1;
  for (const std::uint8_t block : blocks) {
    const std::uint8_t sector = block / 4U;
    if (authenticatedSector != static_cast<std::int8_t>(sector)) {
      if (!authenticateMifareBlock(target, block, keys.data() + sector * 6U))
        continue;
      authenticatedSector = static_cast<std::int8_t>(sector);
    }
    if (readMifareBlock(target, block, raw.mifareBlocks[block]))
      raw.mifareBlockMask |= 1UL << block;
  }
}

bool writePage(TargetInfo& target, std::uint8_t page,
               const std::uint8_t* bytes) {
  const std::uint8_t command[] = {kMifareWrite, page, bytes[0], bytes[1],
                                  bytes[2], bytes[3]};
  for (std::uint8_t attempt = 0; attempt < config::kNtagPageWriteAttempts;
       ++attempt) {
    std::uint8_t response[4]{};
    std::size_t length = 0;
    if (exchange(target, command, sizeof(command), response,
                 sizeof(response), length)) {
      vTaskDelay(pdMS_TO_TICKS(config::kNtagPageWriteSettleMs));
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(config::kNtagPageWriteSettleMs));
    // A failed InDataExchange can invalidate the PN532 logical target. A
    // retry with that stale target number can never recover, so explicitly
    // activate the same physical tag again before retrying the page.
    resetRfField();
    TargetInfo reacquired{};
    bool sameTagReacquired = false;
    for (std::uint8_t scanAttempt = 0;
         scanAttempt < config::kNtagVerificationScanAttempts; ++scanAttempt) {
      if (scanTarget(reacquired) == ScanResult::Found) {
        sameTagReacquired =
            reacquired.uidLength == target.uidLength &&
            std::memcmp(reacquired.uid.data(), target.uid.data(),
                        target.uidLength) == 0;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(config::kNfcScanIntervalMs));
    }
    if (!sameTagReacquired) {
      FS_LOGW(services::LogComponent::Nfc,
              "NTAG write retry aborted page=%u reason=tag_missing_or_uid_changed",
              static_cast<unsigned>(page));
      return false;
    }
    target = reacquired;
  }
  FS_LOGE(services::LogComponent::Nfc,
          "NTAG page write failed page=%u attempts=%u",
          static_cast<unsigned>(page),
          static_cast<unsigned>(config::kNtagPageWriteAttempts));
  return false;
}

bool readNdef(const TargetInfo& target, std::uint8_t* output,
              std::size_t capacity, std::size_t& length) {
  std::uint8_t first[16]{};
  if (!readPages(target, 3, first) || first[0] != 0xE1) return false;
  const std::size_t tagCapacity = static_cast<std::size_t>(first[2]) * 8U;
  const std::size_t maximumLength = std::min(tagCapacity, capacity);
  length = 0;
  std::size_t copied = 0;
  std::uint8_t page = 4;
  while (copied < maximumLength) {
    std::uint8_t block[16]{};
    if (!readPages(target, page, block)) return false;
    const std::size_t count =
        std::min<std::size_t>(16, maximumLength - copied);
    std::memcpy(output + copied, block, count);
    copied += count;
    if (copied == count) {
      bool allZero = true;
      for (std::size_t index = 0; index < copied; ++index) {
        if (output[index] != 0) {
          allZero = false;
          break;
        }
      }
      if (allZero) {
        length = 1;
        return true;
      }
    }
    // Stop as soon as a complete NDEF TLV is available. This avoids reading
    // hundreds of unused bytes from an empty or short NTAG215/216.
    std::size_t cursor = 0;
    while (cursor < copied && output[cursor] == 0x00) ++cursor;
    if (cursor < copied && output[cursor] == 0xFE) {
      length = cursor + 1U;
      return true;
    }
    if (cursor < copied && output[cursor] == 0x03) {
      ++cursor;
      if (cursor >= copied) {
        page = static_cast<std::uint8_t>(page + 4U);
        continue;
      }
      std::size_t payloadLength = output[cursor++];
      if (payloadLength == 0xFF) {
        if (cursor + 2U > copied) {
          page = static_cast<std::uint8_t>(page + 4U);
          continue;
        }
        payloadLength =
            (static_cast<std::size_t>(output[cursor]) << 8U) |
            output[cursor + 1U];
        cursor += 2U;
      }
      if (cursor + payloadLength <= copied) {
        length = cursor + payloadLength;
        if (length < copied && output[length] == 0xFE) ++length;
        return true;
      }
    }
    page = static_cast<std::uint8_t>(page + 4U);
  }
  length = copied;
  return true;
}

bool writeNdef(TargetInfo& target, models::TagTechnology technology,
               const std::uint8_t* bytes, std::size_t length) {
  if (length == 0 || (length % 4) != 0) return false;
  std::uint8_t capability[16]{};
  if (!readPages(target, 3, capability)) return false;
  if (capability[0] == 0 && capability[1] == 0 && capability[2] == 0 &&
      capability[3] == 0) {
    std::uint8_t size = 0;
    if (technology == models::TagTechnology::Ntag213) size = 0x12;
    if (technology == models::TagTechnology::Ntag215) size = 0x3E;
    if (technology == models::TagTechnology::Ntag216) size = 0x6D;
    if (size == 0) return false;
    const std::uint8_t formatted[] = {0xE1, 0x10, size, 0x00};
    if (!writePage(target, 3, formatted)) return false;
    std::memcpy(capability, formatted, sizeof(formatted));
  }
  if (capability[0] != 0xE1 || (capability[3] & 0x0FU) == 0x0FU ||
      length > static_cast<std::size_t>(capability[2]) * 8U)
    return false;
  for (std::size_t offset = 0; offset < length; offset += 4) {
    if (!writePage(target, static_cast<std::uint8_t>(4U + offset / 4U),
                   bytes + offset)) {
      return false;
    }
  }
  return true;
}

models::TagTechnology technologyFor(const TargetInfo& target) {
  if (target.sak == 0x08) return models::TagTechnology::MifareClassic1K;
  if (target.sak == 0x18) return models::TagTechnology::MifareClassic4K;
  return models::TagTechnology::OtherIso14443A;
}

models::TagTechnology technologyFromCapability(const std::uint8_t* capability) {
  if (capability[0] != 0xE1) return models::TagTechnology::OtherIso14443A;
  if (capability[2] == 0x12) return models::TagTechnology::Ntag213;
  if (capability[2] == 0x3E) return models::TagTechnology::Ntag215;
  if (capability[2] == 0x6D) return models::TagTechnology::Ntag216;
  return models::TagTechnology::OtherIso14443A;
}

models::TagTechnology detectNtagTechnology(TargetInfo& target,
                                           bool diagnostics = false) {
  std::uint8_t capability[16]{};
  if (!readPages(target, 3, capability)) {
    if (diagnostics)
      FS_LOGW(services::LogComponent::Nfc,
              "Type2 capability read failed page=3");
    return technologyFor(target);
  }
  const models::TagTechnology capabilityTechnology =
      technologyFromCapability(capability);
  if (capabilityTechnology != models::TagTechnology::OtherIso14443A) {
    if (diagnostics)
      FS_LOGD(services::LogComponent::Nfc,
              "NTAG identified cc=%02X%02X%02X%02X", capability[0],
              capability[1], capability[2], capability[3]);
    return capabilityTechnology;
  }

  // Only an unformatted tag needs the raw GET_VERSION command. Mixing
  // InCommunicateThru and InDataExchange on every normal read invalidated the
  // selected target intermittently on the PN532 HSU interface.
  const bool unformatted = capability[0] == 0 && capability[1] == 0 &&
                           capability[2] == 0 && capability[3] == 0;
  if (!unformatted) return technologyFor(target);
  const std::uint8_t command[] = {kGetVersion};
  std::uint8_t version[8]{};
  std::size_t versionLength = 0;
  if (!communicateThru(command, sizeof(command), version, sizeof(version),
                       versionLength)) {
    if (diagnostics)
      FS_LOGW(services::LogComponent::Nfc, "NTAG GET_VERSION failed");
    return technologyFor(target);
  }
  const models::TagTechnology technology =
      services::identifyNtag21x(version, versionLength, capability);
  if (diagnostics) {
    FS_LOGD(services::LogComponent::Nfc,
        "Unformatted NTAG version=%02X%02X%02X%02X%02X%02X%02X%02X",
        version[0], version[1], version[2], version[3], version[4], version[5],
        version[6], version[7]);
  }
  // Raw communication does not preserve the target handle reliably. Select
  // the same physical tag again before any page operation follows.
  resetRfField();
  TargetInfo reacquired{};
  if (scanTarget(reacquired) != ScanResult::Found ||
      target.uidLength != reacquired.uidLength ||
      std::memcmp(target.uid.data(), reacquired.uid.data(), target.uidLength) !=
          0) {
    return models::TagTechnology::OtherIso14443A;
  }
  target = reacquired;
  return technology;
}

std::uint8_t dynamicLockPage(models::TagTechnology technology) {
  switch (technology) {
    case models::TagTechnology::Ntag213:
      return 0x28;
    case models::TagTechnology::Ntag215:
      return 0x82;
    case models::TagTechnology::Ntag216:
      return 0xE2;
    default:
      return 0;
  }
}

bool ntagWritableForPages(const TargetInfo& target,
                          models::TagTechnology technology,
                          std::uint8_t lastPage, bool diagnostics = false,
                          bool* resultKnown = nullptr) {
  if (resultKnown != nullptr) *resultKnown = false;
  if (dynamicLockPage(technology) == 0) return false;
  std::uint8_t capability[16]{};
  std::uint8_t manufacturerAndLocks[16]{};
  std::uint8_t dynamicLocks[16]{};
  const bool capabilityRead = readPages(target, 3, capability);
  const bool staticLocksRead = readPages(target, 0, manufacturerAndLocks);
  const bool dynamicLocksRead =
      readPages(target, dynamicLockPage(technology), dynamicLocks);
  if (!capabilityRead || !staticLocksRead || !dynamicLocksRead) {
    if (diagnostics)
      FS_LOGW(services::LogComponent::Nfc,
          "Writable metadata incomplete cc=%s static=%s dynamic=%s",
          capabilityRead ? "ok" : "failed",
          staticLocksRead ? "ok" : "failed",
          dynamicLocksRead ? "ok" : "failed");
    return false;
  }
  const std::uint8_t staticLocks[] = {manufacturerAndLocks[10],
                                      manufacturerAndLocks[11]};
  const bool writable = services::ntag21xRangeWritable(
      technology, lastPage, capability, staticLocks, dynamicLocks,
      // READ(dynamic-lock-page) returns dynamic locks followed by CFG0.
      // AUTH0 is byte 3 of CFG0, therefore offset 4 + 3 in this block.
      dynamicLocks[7]);
  if (resultKnown != nullptr) *resultKnown = true;
  if (diagnostics)
    FS_LOGD(services::LogComponent::Nfc,
        "NTAG locks static=%02X%02X dynamic=%02X%02X%02X auth0=%02X writable=%s",
        staticLocks[0], staticLocks[1], dynamicLocks[0], dynamicLocks[1],
        dynamicLocks[2], dynamicLocks[7], writable ? "writable" : "locked");
  return writable;
}

rtos::NfcTagType legacyType(models::TagFormat format) {
  switch (format) {
    case models::TagFormat::FilamentStation:
      return rtos::NfcTagType::Spoolman;
    case models::TagFormat::BambuLab:
      return rtos::NfcTagType::Bambu;
    case models::TagFormat::Legacy:
      return rtos::NfcTagType::Legacy;
    default:
      return rtos::NfcTagType::Unknown;
  }
}

const char* formatDescription(models::TagFormat format) {
  switch (format) {
    case models::TagFormat::EmptyNdef:
      return "leeres NDEF";
    case models::TagFormat::FilamentStation:
      return "FilamentStation";
    case models::TagFormat::BambuLab:
      return "BambuLab";
    case models::TagFormat::OpenPrintTag:
      return "OpenPrintTag";
    case models::TagFormat::OpenTag3D:
      return "OpenTag3D";
    case models::TagFormat::Legacy:
      return "Legacy";
    case models::TagFormat::Unknown:
    default:
      return "unbekannt";
  }
}

const char* technologyDescription(models::TagTechnology technology) {
  switch (technology) {
    case models::TagTechnology::Ntag213:
      return "NTAG213";
    case models::TagTechnology::Ntag215:
      return "NTAG215";
    case models::TagTechnology::Ntag216:
      return "NTAG216";
    case models::TagTechnology::MifareClassic1K:
      return "MIFARE Classic 1K";
    case models::TagTechnology::MifareClassic4K:
      return "MIFARE Classic 4K";
    case models::TagTechnology::OtherIso14443A:
      return "ISO14443A";
    case models::TagTechnology::Unknown:
    default:
      return "unbekannt";
  }
}

void fillTarget(rtos::AppEvent& event, const TargetInfo& target) {
  event.nfcUidLength = target.uidLength;
  std::memcpy(event.nfcUid, target.uid.data(), target.uidLength);
}

void formatUid(const TargetInfo& target, char* output, std::size_t capacity) {
  if (capacity == 0) return;
  output[0] = '\0';
  std::size_t used = 0;
  for (std::uint8_t index = 0; index < target.uidLength; ++index) {
    const int written = std::snprintf(output + used, capacity - used, "%s%02X",
                                      index == 0 ? "" : ":",
                                      target.uid[index]);
    if (written < 0 || static_cast<std::size_t>(written) >= capacity - used) {
      output[capacity - 1] = '\0';
      return;
    }
    used += static_cast<std::size_t>(written);
  }
}

models::TagReadResult reportTag(rtos::RtosContext& ctx,
                                TargetInfo& target) {
  char uid[config::kNfcMaxUidLength * 3]{};
  formatUid(target, uid, sizeof(uid));

  // static, PSRAM-backed (services/PsramAlloc.h): AppEvent is large and
  // this function's own call chain (MIFARE auth, HKDF-SHA256,
  // TagParserRegistry::parse) is already stack-heavy; a stack-local
  // AppEvent here previously caused a canary-triggered crash once AppEvent
  // grew past the task's stack budget.
  static rtos::AppEvent* detected =
      services::allocatePsramInstance<rtos::AppEvent>(
          "NfcTask.reportTag.detected");
  *detected = rtos::AppEvent{};
  detected->type = rtos::AppEventType::NfcTagDetected;
  fillTarget(*detected, target);
  std::snprintf(detected->text, sizeof(detected->text),
                "Tag erkannt: UID=%s, %u Byte, SAK=%02X", uid,
                target.uidLength, target.sak);
  sendEvent(ctx, *detected);

  // RawTagData contains the complete NDEF buffer and decrypted MIFARE blocks.
  // It is only used by this single NfcTask, so keep it out of the task stack;
  // Bambu key derivation and parser events already need substantial stack at
  // the same point in the call chain.
  static models::RawTagData raw{};
  raw = {};
  raw.technology = technologyFor(target);
  if (raw.technology == models::TagTechnology::OtherIso14443A &&
      target.sak == 0x00) {
    raw.technology = detectNtagTechnology(target, true);
  }
  if (raw.technology == models::TagTechnology::OtherIso14443A &&
      target.sak == 0x00) {
    TargetInfo reacquired{};
    if (scanTarget(reacquired) == ScanResult::Found &&
        reacquired.uidLength == target.uidLength &&
        std::memcmp(reacquired.uid.data(), target.uid.data(),
                    target.uidLength) == 0) {
      target = reacquired;
      FS_LOGD(services::LogComponent::Nfc,
              "Target reacquired after_get_version_failure=true");
    }
  }
  raw.uidLength = target.uidLength;
  raw.sak = target.sak;
  std::memcpy(raw.uid, target.uid.data(), target.uidLength);
  readBambuBlocks(target, raw);
  const bool nativeNtag =
      raw.technology == models::TagTechnology::Ntag213 ||
      raw.technology == models::TagTechnology::Ntag215 ||
      raw.technology == models::TagTechnology::Ntag216;
  if (nativeNtag) {
    // Read and classify the payload before optional high-page lock metadata.
    // A failure at a distant dynamic-lock page must not invalidate an
    // otherwise readable native NDEF payload.
    std::size_t ndefLength = 0;
    if (readNdef(target, raw.ndef, sizeof(raw.ndef), ndefLength)) {
      raw.ndefPresent = true;
      raw.ndefReadable = true;
      raw.ndefLength = static_cast<std::uint16_t>(ndefLength);
    }
    // A factory-empty native NTAG has no NDEF container yet. Determine its
    // physical capability without formatting or changing it here.
    raw.hardwareWritable = ntagWritableForPages(
        target, raw.technology, 14, true, &raw.hardwareWritableKnown);
  }
  static const nfc::TagParserRegistry registry{};
  const models::TagReadResult result = registry.parse(raw);
  FS_LOGI(services::LogComponent::Nfc,
          "Tag read uid=%s uid_bytes=%u sak=0x%02X tech=%s format=%s "
          "writable=%s ndef_present=%s spool_id=%lu",
          uid, static_cast<unsigned>(raw.uidLength),
          static_cast<unsigned>(raw.sak),
          technologyDescription(result.technology),
          formatDescription(result.format), result.writable ? "true" : "false",
          result.ndefPresent ? "true" : "false",
          static_cast<unsigned long>(result.definition.hasSpoolId
                                         ? result.definition.spoolId
                                         : 0U));

  // static, PSRAM-backed: see `detected` above / services/PsramAlloc.h.
  static rtos::AppEvent* read =
      services::allocatePsramInstance<rtos::AppEvent>(
          "NfcTask.reportTag.read");
  *read = rtos::AppEvent{};
  read->type = rtos::AppEventType::NfcTagRead;
  fillTarget(*read, target);
  read->tagReadResult = result;
  read->nfcTagType = legacyType(result.format);
  read->spoolId = result.definition.hasSpoolId ? result.definition.spoolId : 0;
  if (result.technology == models::TagTechnology::MifareClassic1K ||
      result.technology == models::TagTechnology::MifareClassic4K) {
    std::snprintf(read->text, sizeof(read->text),
                  "UID=%s: MIFARE Classic, Inhalt nicht gelesen", uid);
  } else if (!result.ndefReadable) {
    std::snprintf(read->text, sizeof(read->text),
                  "UID=%s: kein lesbares Type-2-NDEF", uid);
  } else if (!result.payloadValid) {
    std::snprintf(read->text, sizeof(read->text),
                  "UID=%s: ungueltige FilamentStation-Payload", uid);
  } else if (result.definition.hasSpoolId) {
    std::snprintf(read->text, sizeof(read->text), "UID=%s: %s, ID=%lu", uid,
                  formatDescription(result.format),
                  static_cast<unsigned long>(result.definition.spoolId));
  } else {
    std::snprintf(read->text, sizeof(read->text), "UID=%s: %s (%u Byte)", uid,
                  formatDescription(result.format),
                  static_cast<unsigned>(raw.ndefLength));
  }
  sendEvent(ctx, *read);
  return result;
}

bool sameUid(const TargetInfo& left, const TargetInfo& right) {
  return left.uidLength == right.uidLength &&
         std::memcmp(left.uid.data(), right.uid.data(), left.uidLength) == 0;
}

void handleWrite(rtos::RtosContext& ctx, const rtos::NfcCommand& command,
                 TargetInfo& target, models::TagReadResult& activeResult) {
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> bytes{};
  std::size_t length = 0;
  TargetInfo writeTarget = target;
  const models::TagTechnology technology = detectNtagTechnology(writeTarget);
  if (!services::buildSpoolmanType2Ndef(command.spoolId, bytes.data(),
                                        bytes.size(), length)) {
    FS_LOGE(services::LogComponent::Nfc,
            "NTAG payload creation failed spool_id=%lu",
            static_cast<unsigned long>(command.spoolId));
    sendError(ctx, command.requestId, "NFC tag payload creation failed");
    return;
  }
  const bool payloadAlreadyMatches =
      activeResult.format == models::TagFormat::FilamentStation &&
      activeResult.definition.hasSpoolId &&
      activeResult.definition.spoolId == command.spoolId;
  if (payloadAlreadyMatches) {
    FS_LOGI(services::LogComponent::Nfc,
        "Payload already current spool_id=%lu action=verify_only",
        static_cast<unsigned long>(command.spoolId));
  } else {
    if (!ntagWritableForPages(
            writeTarget, technology,
            static_cast<std::uint8_t>(3U + length / 4U), true)) {
      FS_LOGE(services::LogComponent::Nfc,
              "NTAG write rejected reason=unsafe_range");
      sendError(ctx, command.requestId,
                "NFC tag write range is not writable");
      return;
    }
    if (!writeNdef(writeTarget, technology, bytes.data(), length)) {
      FS_LOGE(services::LogComponent::Nfc, "NTAG NDEF write failed");
      sendError(ctx, command.requestId, "NFC tag write failed");
      return;
    }
  }
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> verify{};
  std::size_t verifyLength = 0;
  TargetInfo verifiedTarget{};
  // InListPassiveTarget is a fresh activation command. Release the logical
  // target used for writing before verification, otherwise the PN532 can
  // report no new target even though the tag is still physically present.
  resetRfField();
  bool sameTag = false;
  for (std::uint8_t attempt = 0;
       attempt < config::kNtagVerificationScanAttempts; ++attempt) {
    const ScanResult scanResult = scanTarget(verifiedTarget);
    if (scanResult == ScanResult::Found) {
      sameTag = sameUid(writeTarget, verifiedTarget);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(config::kNfcScanIntervalMs));
  }
  const auto result = sameTag &&
                              readNdef(verifiedTarget, verify.data(),
                                       verify.size(), verifyLength)
                          ? services::parseType2Ndef(verify.data(), verifyLength)
                          : services::NfcPayloadInfo{};
  if (!sameTag || result.type != services::NfcPayloadType::Spoolman ||
      result.spoolId != command.spoolId) {
    FS_LOGE(services::LogComponent::Nfc,
        "NTAG verification failed same_uid=%s payload_type=%u expected_spool_id=%lu actual_spool_id=%lu bytes=%u",
        sameTag ? "yes" : "no", static_cast<unsigned>(result.type),
        static_cast<unsigned long>(command.spoolId),
        static_cast<unsigned long>(result.spoolId),
        static_cast<unsigned>(verifyLength));
    sendError(ctx, command.requestId, "NFC tag verification failed");
    return;
  }
  // static, PSRAM-backed: see reportTag()'s `detected` above /
  // services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "NfcTask.handleWrite");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::NfcTagWritten;
  event->requestId = command.requestId;
  event->spoolId = command.spoolId;
  event->nfcTagType = rtos::NfcTagType::Spoolman;
  event->tagReadResult = activeResult;
  event->tagReadResult.format = models::TagFormat::FilamentStation;
  event->tagReadResult.knownFormat = true;
  event->tagReadResult.ndefPresent = true;
  event->tagReadResult.ndefReadable = true;
  event->tagReadResult.payloadValid = true;
  event->tagReadResult.writable = true;
  event->tagReadResult.erasable = true;
  event->tagReadResult.definition.format = models::TagFormat::FilamentStation;
  event->tagReadResult.definition.hasSpoolId = true;
  event->tagReadResult.definition.spoolId = command.spoolId;
  event->tagReadResult.uidLength = verifiedTarget.uidLength;
  std::memcpy(event->tagReadResult.uid, verifiedTarget.uid.data(),
              verifiedTarget.uidLength);
  nfc::updateTagCapabilities(event->tagReadResult);
  target = verifiedTarget;
  activeResult = event->tagReadResult;
  fillTarget(*event, verifiedTarget);
  std::snprintf(event->text, sizeof(event->text), "NFC tag written and verified");
  sendEvent(ctx, *event);
}

void handleErase(rtos::RtosContext& ctx, const rtos::NfcCommand& command,
                 TargetInfo& target, models::TagReadResult& activeResult) {
  const std::uint8_t empty[] = {0x03, 0x00, 0xFE, 0x00};
  static std::array<std::uint8_t, config::kNfcMaxNdefBytes> verify{};
  std::size_t length = 0;
  TargetInfo verifiedTarget{};
  TargetInfo eraseTarget = target;
  if (!writeNdef(eraseTarget, detectNtagTechnology(eraseTarget), empty,
                 sizeof(empty)) ||
      scanTarget(verifiedTarget) != ScanResult::Found ||
      !sameUid(target, verifiedTarget) ||
      !readNdef(verifiedTarget, verify.data(), verify.size(), length) ||
      services::parseType2Ndef(verify.data(), length).type !=
          services::NfcPayloadType::Empty) {
    sendError(ctx, command.requestId, "NFC tag erase verification failed");
    return;
  }
  // static, PSRAM-backed: see reportTag()'s `detected` above /
  // services/PsramAlloc.h.
  static rtos::AppEvent* event =
      services::allocatePsramInstance<rtos::AppEvent>(
          "NfcTask.handleErase");
  *event = rtos::AppEvent{};
  event->type = rtos::AppEventType::NfcTagErased;
  event->requestId = command.requestId;
  fillTarget(*event, verifiedTarget);
  target = verifiedTarget;
  activeResult.format = models::TagFormat::EmptyNdef;
  activeResult.knownFormat = true;
  activeResult.ndefPresent = true;
  activeResult.ndefReadable = true;
  activeResult.payloadValid = true;
  activeResult.definition = {};
  nfc::updateTagCapabilities(activeResult);
  std::snprintf(event->text, sizeof(event->text), "NFC tag erased and verified");
  sendEvent(ctx, *event);
}

}  // namespace

void nfcTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  if (!initializeUart()) {
    FS_LOGE(services::LogComponent::Nfc,
            "PN532 UART initialization failed uart=%d",
            config::kPn532UartNumber);
    sendError(ctx, 0, "PN532 UART initialization failed");
    for (;;) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
  FS_LOGI(services::LogComponent::Nfc,
          "UART ready uart=%d tx_gpio=%d rx_gpio=%d baud=%lu mode=8N1",
          config::kPn532UartNumber, config::kPn532UartTxPin,
          config::kPn532UartRxPin,
          static_cast<unsigned long>(config::kPn532UartBaudRate));
  const Pn532InitializationResult initializationResult = initializePn532();
  if (initializationResult != Pn532InitializationResult::Ready) {
    if (lastTransactionRxBytes == 0) {
      FS_LOGE(services::LogComponent::Nfc,
              "PN532 initialization failed step=%s uart_bytes=0",
              initializationStep(initializationResult));
      sendError(ctx, 0, "PN532 not responding; check HSU wiring");
    } else {
      FS_LOGE(services::LogComponent::Nfc,
              "PN532 response invalid step=%s uart_bytes=%u",
              initializationStep(initializationResult),
              static_cast<unsigned>(lastTransactionRxBytes));
      sendError(ctx, 0, "PN532 returned an invalid UART frame");
    }
  } else {
    FS_LOGI(services::LogComponent::Nfc, "PN532 ready");
    xEventGroupSetBits(ctx.systemEventGroup, rtos::EVENT_NFC_READY);
    // static, PSRAM-backed: see reportTag()'s `detected` above /
    // services/PsramAlloc.h.
    static rtos::AppEvent* event =
        services::allocatePsramInstance<rtos::AppEvent>(
            "NfcTask.nfcTask.initialized");
    *event = rtos::AppEvent{};
    event->type = rtos::AppEventType::NfcInitialized;
    std::snprintf(event->text, sizeof(event->text), "PN532 ready");
    sendEvent(ctx, *event);
  }

  FS_LOGD(services::LogComponent::Nfc,
          "Stack watermark context=initialization free_bytes=%u",
          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));

  bool reading = true;
  bool present = false;
  TargetInfo active{};
  models::TagReadResult activeResult{};
  std::uint32_t lastSeenMs = 0;
  std::uint8_t consecutiveScanErrors = 0;
  std::uint8_t confirmedAbsenceScans = 0;
  std::uint8_t confirmedFreshAbsenceScans = 0;
  bool pn532Connected =
      initializationResult == Pn532InitializationResult::Ready;
  std::uint16_t sustainedCommErrors = 0;
  bool poweredDown = false;
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
        case rtos::NfcCommandType::PowerDown:
          if (!poweredDown) {
            poweredDown = true;
            setRfField(false);
            if (!powerDown()) {
              FS_LOGW(services::LogComponent::Nfc,
                      "PN532 PowerDown command failed, RF field already off");
            } else {
              FS_LOGI(services::LogComponent::Nfc, "PN532 powered down");
            }
            xEventGroupClearBits(ctx.systemEventGroup, rtos::EVENT_NFC_READY);
            pn532Connected = false;
            present = false;
            active = {};
            activeResult = {};
            sendPowerAck(ctx);
          }
          break;
        case rtos::NfcCommandType::PowerUp:
          if (poweredDown) {
            poweredDown = false;
            const Pn532InitializationResult wakeResult = initializePn532();
            if (wakeResult == Pn532InitializationResult::Ready) {
              pn532Connected = true;
              sustainedCommErrors = 0;
              reportPn532Reconnected(ctx);
            } else {
              // Kein Sonderfall noetig: der naechste scanTarget()-Aufruf
              // schlaegt einfach fehl und die bestehende Comm-Error-/
              // Reconnect-Erkennung (notePn532CommError -> sustained ->
              // reportPn532Disconnected) uebernimmt wie bei jeder anderen
              // Transportstoerung.
              FS_LOGW(services::LogComponent::Nfc,
                      "PN532 wake re-init failed step=%s",
                      initializationStep(wakeResult));
            }
          }
          break;
        case rtos::NfcCommandType::WriteSpoolTag:
          if (present && nfc::mayWriteTag(activeResult))
            handleWrite(ctx, command, active, activeResult);
          else if (present)
            sendError(ctx, command.requestId, "Tag format is read-only");
          else
            sendError(ctx, command.requestId, "No NFC tag present");
          break;
        case rtos::NfcCommandType::EraseTag:
          if (present && nfc::mayEraseTag(activeResult))
            handleErase(ctx, command, active, activeResult);
          else if (present)
            sendError(ctx, command.requestId, "Tag format cannot be erased");
          else
            sendError(ctx, command.requestId, "No NFC tag present");
          break;
      }
      continue;
    }
    if (poweredDown) continue;
    if (!reading) continue;

    if (present) {
      // InAutoPoll is a discovery/activation command. A tag which is already
      // selected must be checked through its current logical target instead;
      // repeatedly polling it can legitimately yield no new target.
      std::uint8_t capability[16]{};
      if (readPages(active, 3, capability)) {
        lastSeenMs = millis();
        confirmedAbsenceScans = 0;
        confirmedFreshAbsenceScans = 0;
        consecutiveScanErrors = 0;
        notePn532Responding(ctx, pn532Connected, sustainedCommErrors);
        continue;
      }

      ++confirmedAbsenceScans;
      if (millis() - lastSeenMs < config::kNfcTagRemovalDelayMs ||
          confirmedAbsenceScans < config::kNfcRemovalConfirmationScans) {
        continue;
      }

      // Before emitting removal, perform one fresh RF activation. This
      // distinguishes a physically removed tag from a lost PN532 target
      // handle after a failed data exchange.
      const TargetInfo removalCandidate = active;
      resetRfField();
      TargetInfo reacquired{};
      const ScanResult reacquireResult = scanTarget(reacquired);
      if (reacquireResult == ScanResult::Found &&
          sameUid(removalCandidate, reacquired)) {
        active = reacquired;
        lastSeenMs = millis();
        confirmedAbsenceScans = 0;
        confirmedFreshAbsenceScans = 0;
        consecutiveScanErrors = 0;
        notePn532Responding(ctx, pn532Connected, sustainedCommErrors);
        continue;
      }
      if (reacquireResult == ScanResult::CommunicationError) {
        // Transport/PN532 errors do not prove that the RF tag disappeared.
        confirmedFreshAbsenceScans = 0;
        ++consecutiveScanErrors;
        notePn532CommError(ctx, pn532Connected, sustainedCommErrors);
        continue;
      }
      // A NoTarget result still proves the PN532 itself answered.
      notePn532Responding(ctx, pn532Connected, sustainedCommErrors);

      if (reacquireResult == ScanResult::NoTarget &&
          ++confirmedFreshAbsenceScans <
              config::kNfcFreshAbsenceConfirmationScans) {
        continue;
      }

      // static, PSRAM-backed: see reportTag()'s `detected` above /
      // services/PsramAlloc.h.
      static rtos::AppEvent* event =
          services::allocatePsramInstance<rtos::AppEvent>(
              "NfcTask.nfcTask.tagRemoved");
      *event = rtos::AppEvent{};
      event->type = rtos::AppEventType::NfcTagRemoved;
      fillTarget(*event, removalCandidate);
      char uid[config::kNfcMaxUidLength * 3]{};
      formatUid(removalCandidate, uid, sizeof(uid));
      std::snprintf(event->text, sizeof(event->text), "Tag entfernt: UID=%s", uid);
      FS_LOGI(services::LogComponent::Nfc, "Tag removed uid=%s", uid);
      sendEvent(ctx, *event);
      present = false;
      active = {};
      activeResult = {};
      confirmedAbsenceScans = 0;
      confirmedFreshAbsenceScans = 0;
      consecutiveScanErrors = 0;
      // If another tag was already found during removal confirmation, keep
      // its target information for the next iteration instead of losing it.
      if (reacquireResult == ScanResult::Found) {
        active = reacquired;
        present = true;
        activeResult = reportTag(ctx, active);
        lastSeenMs = millis();
        confirmedFreshAbsenceScans = 0;
      }
      continue;
    }

    TargetInfo found{};
    const ScanResult scanResult = scanTarget(found);
    if (scanResult == ScanResult::Found) {
      active = found;
      present = true;
      activeResult = reportTag(ctx, active);
      // Start removal timing only after the potentially lengthy initial tag
      // analysis has finished.
      lastSeenMs = millis();
      consecutiveScanErrors = 0;
      confirmedAbsenceScans = 0;
      confirmedFreshAbsenceScans = 0;
      notePn532Responding(ctx, pn532Connected, sustainedCommErrors);
    } else if (scanResult == ScanResult::CommunicationError) {
      // A malformed or timed-out UART transaction is not evidence that the
      // tag was removed. Recover the PN532 transport, but preserve presence.
      notePn532CommError(ctx, pn532Connected, sustainedCommErrors);
      if (++consecutiveScanErrors >= 2) {
        FS_LOGW(services::LogComponent::Nfc,
                "PN532 scan recovery consecutive_errors=%u",
                static_cast<unsigned>(consecutiveScanErrors));
        resetRfField();
        consecutiveScanErrors = 0;
      }
    } else {
      consecutiveScanErrors = 0;
      notePn532Responding(ctx, pn532Connected, sustainedCommErrors);
    }
  }
}

}  // namespace filament_station::tasks
