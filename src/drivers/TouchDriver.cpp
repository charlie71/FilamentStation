#include "drivers/TouchDriver.h"

#include "drivers/DisplayDriver.h"

namespace filament_station::drivers {

bool readTouchCoordinates(std::int32_t& x, std::int32_t& y) {
  return displayDevice().getTouch(&x, &y);
}

}  // namespace filament_station::drivers
