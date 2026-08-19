#include "services/Logger.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "rtos/RtosContext.h"
#include "config/TaskConfig.h"
#include "services/LoggerFormat.h"

namespace filament_station::services {
namespace {

static_assert(config::kLogMessageCapacity >= 64,
              "Log message capacity is too small for useful diagnostics");

}  // namespace

void Logger::log(LogLevel level, LogComponent component, const char* format,
                 ...) {
  // Logging may block briefly while enqueueing and is therefore forbidden in
  // interrupt context. ISRs must notify a task which performs the log instead.
  if (xPortInIsrContext() || !enabled(level) || format == nullptr) return;

  char message[config::kLogMessageCapacity]{};
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(message, sizeof(message), format, arguments);
  va_end(arguments);

  char line[config::kLogMessageCapacity]{};
  const std::size_t length =
      formatLogRecord(line, sizeof(line), level, component, message);
  if (length == 0) return;

  // Before the RTOS logger exists there is only the Arduino setup task, so a
  // direct emergency write is safe and preserves startup diagnostics. During
  // normal operation the queue and its sole writer keep lines atomic.
  const auto& context = rtos::context();
  if (context.logQueue == nullptr || context.loggingTask == nullptr) {
    Serial.write(reinterpret_cast<const std::uint8_t*>(line), length);
    return;
  }
  rtos::enqueueLogLine(line);
}

void Logger::task(void* parameter) {
  auto& context = *static_cast<rtos::RtosContext*>(parameter);
  static rtos::LogMessage message{};
  // Keep each HWCDC transfer below one 64-byte USB full-speed packet. Some
  // ESP32-S3 HWCDC revisions can acknowledge a larger write while dropping an
  // internal packet during dense startup bursts.
  constexpr std::size_t kUsbCdcChunkSize = 48;
  for (;;) {
    if (xQueueReceive(context.logQueue, &message, portMAX_DELAY) != pdPASS)
      continue;
    const std::size_t length = strnlen(message.text, sizeof(message.text));
    std::size_t written = 0;
    while (written < length) {
      const std::size_t remaining = length - written;
      const std::size_t requested =
          remaining < kUsbCdcChunkSize ? remaining : kUsbCdcChunkSize;
      const std::size_t count = Serial.write(
          reinterpret_cast<const std::uint8_t*>(message.text) + written,
          requested);
      if (count == 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
      } else {
        written += count;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
    Serial.flush();
  }
}

}  // namespace filament_station::services
