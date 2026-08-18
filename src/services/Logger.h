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

namespace filament_station::services {

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
};

class Logger final {
 public:
  Logger() = delete;

  static constexpr bool enabled(LogLevel level) {
    return static_cast<std::uint8_t>(level) <= FS_LOG_LEVEL;
  }

  static void log(LogLevel level, LogComponent component, const char* format,
                  ...);

  static const char* levelName(LogLevel level);
  static const char* componentName(LogComponent component);
};

}  // namespace filament_station::services

// Passing the complete printf-style argument list through __VA_ARGS__ keeps
// calls without format parameters valid in C++17 without compiler extensions.
#define FS_LOGE(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Error)) {                  \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Error, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGW(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Warn)) {                   \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Warn, component,             \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGI(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Info)) {                   \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Info, component,             \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGD(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Debug)) {                  \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Debug, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

#define FS_LOGT(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Trace)) {                  \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Trace, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)
