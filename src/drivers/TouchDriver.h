#pragma once

#include <cstdint>

namespace filament_station::drivers {

bool readTouchCoordinates(std::int32_t& x, std::int32_t& y);

}  // namespace filament_station::drivers
