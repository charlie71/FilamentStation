#include "services/Logger.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "rtos/RtosContext.h"
#include "config/TaskConfig.h"

namespace filament_station::services {
namespace {

constexpr std::size_t kPrefixReserve = 20;
static_assert(config::kLogMessageCapacity > kPrefixReserve,
              "Log message capacity is too small for the canonical prefix");

}  // namespace

const char* Logger::levelName(LogLevel level) {
  switch (level) {
    case LogLevel::Error:
      return "E";
    case LogLevel::Warn:
      return "W";
    case LogLevel::Info:
      return "I";
    case LogLevel::Debug:
      return "D";
    case LogLevel::Trace:
      return "T";
  }
  return "?";
}

const char* Logger::componentName(LogComponent component) {
  switch (component) {
    case LogComponent::App:
      return "APP";
    case LogComponent::Rtos:
      return "RTOS";
    case LogComponent::Ui:
      return "UI";
    case LogComponent::Display:
      return "DISPLAY";
    case LogComponent::Touch:
      return "TOUCH";
    case LogComponent::Storage:
      return "STORAGE";
    case LogComponent::Net:
      return "NET";
    case LogComponent::Spoolman:
      return "SPOOLMAN";
    case LogComponent::Scale:
      return "SCALE";
    case LogComponent::Nfc:
      return "NFC";
    case LogComponent::Bambu:
      return "BAMBU";
  }
  return "UNKNOWN";
}

void Logger::log(LogLevel level, LogComponent component, const char* format,
                 ...) {
  // Logging may block briefly while enqueueing and is therefore forbidden in
  // interrupt context. ISRs must notify a task which performs the log instead.
  if (xPortInIsrContext() || !enabled(level) || format == nullptr) return;

  char line[config::kLogMessageCapacity]{};
  const int prefixLength =
      std::snprintf(line, sizeof(line), "%s [%s] ", levelName(level),
                    componentName(component));
  if (prefixLength < 0) return;

  const std::size_t used = static_cast<std::size_t>(prefixLength);
  if (used >= sizeof(line)) return;

  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(line + used, sizeof(line) - used, format, arguments);
  va_end(arguments);

  // One logger call always produces one physical line. Besides keeping the
  // monitor parseable, this prevents unprefixed continuation lines.
  for (char* character = line + used; *character != '\0'; ++character) {
    if (*character == '\r' || *character == '\n' || *character == '\t')
      *character = ' ';
  }

  // Before the RTOS logger exists there is only the Arduino setup task, so a
  // direct emergency write is safe and preserves startup diagnostics. During
  // normal operation the queue and its sole writer keep lines atomic.
  const auto& context = rtos::context();
  if (context.logQueue == nullptr || context.loggingTask == nullptr) {
    Serial.write(reinterpret_cast<const std::uint8_t*>(line),
                 std::strlen(line));
    Serial.write(reinterpret_cast<const std::uint8_t*>("\r\n"), 2);
    return;
  }
  rtos::enqueueLogLine(line);
}

}  // namespace filament_station::services
