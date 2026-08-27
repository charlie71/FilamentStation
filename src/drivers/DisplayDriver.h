/**
 * @file
 * @brief LovyanGFX display/touch panel driver: initialization and access
 *        to the single global display device instance.
 */
#pragma once

#include <LovyanGFX.hpp>

namespace filament_station::drivers {

/// @brief Initializes the ST7796 panel, backlight, and touch controller.
/// @return true if initialization succeeded and the reported panel size matches config::kDisplayWidth/kDisplayHeight.
bool initializeDisplay();
/// @brief Draws a static color-bar test pattern with a text overlay, used for hardware bring-up.
void drawDisplayColorTest();
/// @brief The single global LovyanGFX display device instance.
/// @return Reference to the display device, usable for drawing and LVGL flush callbacks.
lgfx::LGFX_Device& displayDevice();

}  // namespace filament_station::drivers
