/**
 * @file
 * @brief Central logging facade: level/component filtering, the
 *        FS_LOG*() macros every task must use, and the sole serial-writer
 *        task (services::Logger::task()).
 */
#pragma once

#include <cstddef>
#include <cstdint>

/// @def FS_LOG_LEVEL
/// @brief Maximum compiled verbosity. Override with -DFS_LOG_LEVEL=1..5
///        (1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE).
// Maximum compiled verbosity. Override with -DFS_LOG_LEVEL=1..5.
// 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE.
#ifndef FS_LOG_LEVEL
#define FS_LOG_LEVEL 4
#endif

static_assert(FS_LOG_LEVEL >= 1 && FS_LOG_LEVEL <= 5,
              "FS_LOG_LEVEL must be between 1 (ERROR) and 5 (TRACE)");

namespace filament_station {
namespace services {

/// @brief Log message severity, ordered from most (Error) to least
///        (Trace) important.
enum class LogLevel : std::uint8_t {
  Error = 1,
  Warn = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
};

/// @brief Which subsystem a log line originates from; used both for the
///        `[TAG]` prefix and per-component verbosity restriction.
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

/// @brief Stateless logging facade: level/component filtering, formatting,
///        and the single task that owns the serial (USB CDC) output.
class Logger final {
 public:
  Logger() = delete;

  /// @brief Whether a level is enabled under the compile-time FS_LOG_LEVEL ceiling.
  /// @param level Level to check.
  /// @return true if `level` is at or above the configured verbosity.
  static constexpr bool enabled(LogLevel level) {
    return static_cast<std::uint8_t>(level) <= FS_LOG_LEVEL;
  }

  /// @brief Per-component verbosity ceiling, on top of the global FS_LOG_LEVEL.
  /// @param component Component to look up.
  /// @return The most verbose level still enabled for `component`.
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

  /// @brief Whether a level is enabled for a specific component, combining
  ///        the global ceiling and #componentMinimumLevel().
  /// @param level Level to check.
  /// @param component Originating component.
  /// @return true if the line would actually be emitted.
  static constexpr bool enabled(LogLevel level, LogComponent component) {
    return static_cast<std::uint8_t>(level) <= FS_LOG_LEVEL &&
          static_cast<std::uint8_t>(level) <=
              static_cast<std::uint8_t>(componentMinimumLevel(component));
  }

  /// @brief Formats and emits one log line. Use the FS_LOGE/W/I/D/T macros
  ///        instead of calling this directly, so filtering and metadata
  ///        stay consistent.
  /// @param level Severity level.
  /// @param component Originating subsystem.
  /// @param format printf-style format string.
  /// @note Must not be called from interrupt context; a no-op if it is.
  static void log(LogLevel level, LogComponent component, const char* format,
                  ...);

  /// @brief Sole serial writer task: drains the log queue and writes each
  ///        line to USB CDC in small chunks.
  /// @param parameter Pointer to the owning rtos::RtosContext.
  // Sole serial writer task. RtosContext only creates and owns its handle.
  static void task(void* parameter);

  /// @brief Short text tag for a log level (e.g. "E", "W").
  /// @param level Severity level.
  /// @return Single-character level tag.
  static const char* levelName(LogLevel level);
  /// @brief Short text tag for a log component (e.g. "APP", "NFC").
  /// @param component Originating subsystem.
  /// @return Component tag.
  static const char* componentName(LogComponent component);
};

}  // namespace services
}  // namespace filament_station

// Passing the complete printf-style argument list through __VA_ARGS__ keeps
// calls without format parameters valid in C++17 without compiler extensions.
/// @def FS_LOGE
/// @brief Logs a printf-style message at LogLevel::Error, if enabled for `component`.
#define FS_LOGE(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Error, component)) {       \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Error, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

/// @def FS_LOGW
/// @brief Logs a printf-style message at LogLevel::Warn, if enabled for `component`.
#define FS_LOGW(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Warn, component)) {        \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Warn, component,             \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

/// @def FS_LOGI
/// @brief Logs a printf-style message at LogLevel::Info, if enabled for `component`.
#define FS_LOGI(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Info, component)) {        \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Info, component,             \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

/// @def FS_LOGD
/// @brief Logs a printf-style message at LogLevel::Debug, if enabled for `component`.
#define FS_LOGD(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Debug, component)) {       \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Debug, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)

/// @def FS_LOGT
/// @brief Logs a printf-style message at LogLevel::Trace, if enabled for `component`.
#define FS_LOGT(component, ...)                                                \
  do {                                                                         \
    if (::filament_station::services::Logger::enabled(                          \
            ::filament_station::services::LogLevel::Trace, component)) {       \
      ::filament_station::services::Logger::log(                               \
          ::filament_station::services::LogLevel::Trace, component,            \
          __VA_ARGS__);                                                         \
    }                                                                          \
  } while (false)
