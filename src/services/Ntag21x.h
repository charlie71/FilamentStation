#pragma once

#include <cstddef>
#include <cstdint>

#include "models/TagDefinition.h"

namespace filament_station {
namespace services {

models::TagTechnology identifyNtag21x(const std::uint8_t* version,
                                      std::size_t versionLength,
                                      const std::uint8_t* capability);

bool ntag21xRangeWritable(models::TagTechnology technology,
                          std::uint8_t lastPage,
                          const std::uint8_t* capability,
                          const std::uint8_t* staticLocks,
                          const std::uint8_t* dynamicLocks,
                          std::uint8_t auth0);

}  // namespace services
}  // namespace filament_station
