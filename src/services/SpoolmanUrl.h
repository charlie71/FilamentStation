#pragma once

#include <cstddef>

namespace filament_station {
namespace services {

struct SpoolmanUrlParts {
  char protocol[8]{};
  char host[64]{};
  char port[8]{};
  char basePath[32]{};
};

bool buildNormalizedSpoolmanUrl(const SpoolmanUrlParts& parts, char* output,
                                std::size_t outputCapacity);
bool parseNormalizedSpoolmanUrl(const char* url, SpoolmanUrlParts& parts);

}  // namespace services
}  // namespace filament_station
