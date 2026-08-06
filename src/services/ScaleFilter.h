#pragma once

#include <cstdint>

namespace filament_station::services {

// Phase 4.2 defines the filter boundary. Stateful filtering and stability
// detection are added in phase 4.3; until then raw counts pass through.
class ScaleFilter {
 public:
  std::int32_t process(std::int32_t rawCounts) { return rawCounts; }
};

}  // namespace filament_station::services
