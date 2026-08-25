#pragma once

#include <cstddef>
#include <cstdint>

// Maximum compiled verbosity. Override with -DFS_LOG_LEVEL=1..5.
// 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE.
#ifndef FS_LOG_LEVEL
#define FS_LOG_LEVEL 4
#endif

static_assert(FS_LOG_LEVEL >= 1 && FS_LOG_LEVEL <= 5,
              "FS_LOG_LEVEL must be between 1 (ERROR) and 5 (TRACE)");

namespace filament_station {
namespace services {

enum class LogLevel : std::uint8_t {
  Error = 1,
  Warn = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
};

enum class LogComponent : std::uint8_t {
  App,
  Rtos,
  Ui,
  Display,
  Touch,
  Storage,
  Net,
  Spoolman,
  Scale,
  Nfc,
  Bambu,
  Power,
  Update,
};

class Logger final {
 public:
  Logger() = delete;

  static constexpr bool enabled(LogLevel level) {
    return static_cast<std::uint8_t>(level) <= FS_LOG_LEVEL;
  }

  // Per-component ceiling on top of the global FS_LOG_LEVEL, for silencing
  // one noisy component (e.g. UI's per-command DEBUG lines drowning out
  // Bambu diagnostics on the same serial monitor) without lowering
  // verbosity everywhere. Edit componentMinimumLevel() below to change
  // which components are restricted; there is no runtime/settings-driven
  // override, this is a compile-time diagnostic knob.
  static constexpr LogLevel componentMinimumLevel(LogComponent component) {
    switch (component) {
      case LogComponent::Ui:
        return LogLevel::Error;
      default:
        return static_cast<LogLevel>(FS_LOG_LEVEL);
    }
  }

  static constexpr bool enabled(LogLevel level, LogComponent component) {
    return static_cast<std::uint8_t>(level) <= FS_LOG_LEVEL &&
          static_cast<std::uint8_t>(level) <=
              static_cast<std::uint8_t>(componentMinimumLevel(component));
  }

  static void log(LogLevel level, LogComponent component, const char* format,
                  ...);

  // Sole serial writer task. RtosContext only creates and owns its handle.
  static void task(void* parameter);

  static const char* levelName(LogLevel level);
  static const char* componentName(LogComponent component);
};

}  // namespace services
}  // namespace filament_station

// Passing the complete printf-style argument list through __VA_ARGS__ keeps
// calls without format parameters valid in C++17 without compiler extensions.
#define FS_LOGE(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Error, component)) {       \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Error, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGW(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Warn, component)) {        \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Warn, component,             \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGI(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Info, component)) {        \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Info, component,             \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGD(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Debug, component)) {       \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Debug, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGT(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Trace, component)) {       \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Trace, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)
