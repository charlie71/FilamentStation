#pragma once

#include <LovyanGFX.hpp>

namespace filament_station::drivers {

bool initializeDisplay();
void drawDisplayColorTest();
lgfx::LGFX_Device& displayDevice();

}  // namespace filament_station::drivers
