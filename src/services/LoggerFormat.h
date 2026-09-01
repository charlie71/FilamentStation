/**
 * @file
 * @brief Formats one log record (level, component, message) into the
 *        single-line wire form transported by rtos::LogMessage.
 */
#pragma once

#include <cstddef>

#include "services/Logger.h"

namespace filament_station {
namespace services {

/// @brief Formats one complete transport record, including exactly one LF
///        and the terminating null byte. Embedded line breaks are replaced
///        with spaces.
/// @param output Destination buffer.
/// @param capacity Size of `output` in bytes.
/// @param level Severity level.
/// @param component Originating subsystem.
/// @param message NUL-terminated message text.
/// @return Number of bytes written (including the LF and NUL), or 0 on invalid input/insufficient capacity.
// Formats one complete transport record, including exactly one LF and the
// terminating null byte. Embedded line breaks are replaced with spaces.
std::size_t formatLogRecord(char* output, std::size_t capacity,
                            LogLevel level, LogComponent component,
                            const char* message);

}  // namespace services
}  // namespace filament_station
