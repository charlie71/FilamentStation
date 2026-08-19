#include "services/LoggerFormat.h"

#include <cstdio>

namespace filament_station {
namespace services {
namespace {

const char* levelText(LogLevel level) {
  switch (level) {
    case LogLevel::Error: return "E";
    case LogLevel::Warn: return "W";
    case LogLevel::Info: return "I";
    case LogLevel::Debug: return "D";
    case LogLevel::Trace: return "T";
  }
  return "?";
}

const char* componentText(LogComponent component) {
  switch (component) {
    case LogComponent::App: return "APP";
    case LogComponent::Rtos: return "RTOS";
    case LogComponent::Ui: return "UI";
    case LogComponent::Display: return "DISPLAY";
    case LogComponent::Touch: return "TOUCH";
    case LogComponent::Storage: return "STORAGE";
    case LogComponent::Net: return "NET";
    case LogComponent::Spoolman: return "SPOOLMAN";
    case LogComponent::Scale: return "SCALE";
    case LogComponent::Nfc: return "NFC";
    case LogComponent::Bambu: return "BAMBU";
  }
  return "UNKNOWN";
}

}  // namespace

std::size_t formatLogRecord(char* output, std::size_t capacity,
                            LogLevel level, LogComponent component,
                            const char* message) {
  if (output == nullptr || capacity < 3 || message == nullptr) return 0;

  const int prefixLength = std::snprintf(output, capacity, "%s [%s] ",
                                         levelText(level),
                                         componentText(component));
  if (prefixLength < 0 || static_cast<std::size_t>(prefixLength) > capacity - 2) {
    output[0] = '\0';
    return 0;
  }

  std::size_t used = static_cast<std::size_t>(prefixLength);
  const std::size_t messageLimit = capacity - 2;  // LF plus null byte.
  for (const char* source = message; *source != '\0' && used < messageLimit;
       ++source) {
    const char value = *source;
    output[used++] =
        value == '\r' || value == '\n' || value == '\t' ? ' ' : value;
  }
  output[used++] = '\n';
  output[used] = '\0';
  return used;
}

const char* Logger::levelName(LogLevel level) { return levelText(level); }

const char* Logger::componentName(LogComponent component) {
  return componentText(component);
}

}  // namespace services
}  // namespace filament_station
