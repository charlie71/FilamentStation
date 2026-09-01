/**
 * @file
 * @brief Shared visual constants (colors, font sizes, spacing, layout
 *        bands) for the hand-written UI code, kept consistent with the
 *        EEZ Studio generated screens.
 */
#pragma once

#include <cstdint>

namespace filament_station::ui::design {

/// @brief RGB888 color constants used across the UI.
namespace color {
constexpr std::uint32_t background = 0x101820;  ///< Screen background.
constexpr std::uint32_t surface = 0x1B2A34;      ///< Card/panel surface background.
constexpr std::uint32_t primary = 0x1565C0;      ///< Primary accent/action color.
constexpr std::uint32_t connected = 0x2E7D32;    ///< Positive/connected status color.
constexpr std::uint32_t connecting = 0xF9A825;   ///< In-progress/connecting status color.
constexpr std::uint32_t error = 0xC62828;        ///< Error status color.
constexpr std::uint32_t disabled = 0x616161;     ///< Disabled-state color.
constexpr std::uint32_t text = 0xFFFFFF;         ///< Primary text color.
constexpr std::uint32_t textMuted = 0xB0BEC5;    ///< Secondary/muted text color.
}  // namespace color

/// @brief Font pixel sizes used across the UI.
namespace font {
constexpr std::uint8_t status = 16;  ///< Status-line text size.
constexpr std::uint8_t body = 18;    ///< Body text size.
constexpr std::uint8_t title = 24;   ///< Title text size.
constexpr std::uint8_t weight = 36;  ///< Large weight-display text size.
}  // namespace font

/// @brief Spacing/padding units used across the UI.
namespace spacing {
constexpr std::uint8_t xs = 4;   ///< Extra-small spacing.
constexpr std::uint8_t sm = 8;   ///< Small spacing.
constexpr std::uint8_t md = 12;  ///< Medium spacing.
constexpr std::uint8_t lg = 16;  ///< Large spacing.
}  // namespace spacing

constexpr std::uint16_t minimumTouchHeight = 48;   ///< Minimum touch-target height in pixels.
constexpr std::uint16_t preferredTouchHeight = 56;  ///< Preferred touch-target height in pixels.
constexpr std::uint16_t topPrinterBarHeight = 40;   ///< Height of the top printer-status bar.
constexpr std::uint16_t contentHeight = 224;        ///< Height of the main content band, between the top bar and bottom action bar.
constexpr std::uint16_t bottomActionBarHeight = 56;  ///< Height of the bottom action bar.

static_assert(topPrinterBarHeight + contentHeight + bottomActionBarHeight ==
              320);

}  // namespace filament_station::ui::design
