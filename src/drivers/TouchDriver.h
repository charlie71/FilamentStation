/**
 * @file
 * @brief Thin touch-coordinate read wrapper around the display driver's
 *        embedded touch controller.
 */
#pragma once

#include <cstdint>

namespace filament_station::drivers {

/// @brief Reads the current touch point, if the panel is being touched.
/// @param x Out parameter receiving the touch X coordinate.
/// @param y Out parameter receiving the touch Y coordinate.
/// @return true if a touch is currently detected.
bool readTouchCoordinates(std::int32_t& x, std::int32_t& y);

}  // namespace filament_station::drivers
