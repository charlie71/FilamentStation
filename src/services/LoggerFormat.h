#pragma once

#include <cstddef>

#include "services/Logger.h"

namespace filament_station {
namespace services {

// Formats one complete transport record, including exactly one LF and the
// terminating null byte. Embedded line breaks are replaced with spaces.
std::size_t formatLogRecord(char* output, std::size_t capacity,
                            LogLevel level, LogComponent component,
                            const char* message);

}  // namespace services
}  // namespace filament_station
