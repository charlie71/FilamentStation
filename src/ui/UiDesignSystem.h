#pragma once

#include <cstdint>

namespace filament_station::ui::design {

namespace color {
constexpr std::uint32_t background = 0x101820;
constexpr std::uint32_t surface = 0x1B2A34;
constexpr std::uint32_t primary = 0x1565C0;
constexpr std::uint32_t connected = 0x2E7D32;
constexpr std::uint32_t connecting = 0xF9A825;
constexpr std::uint32_t error = 0xC62828;
constexpr std::uint32_t disabled = 0x616161;
constexpr std::uint32_t text = 0xFFFFFF;
constexpr std::uint32_t textMuted = 0xB0BEC5;
}  // namespace color

namespace font {
constexpr std::uint8_t status = 16;
constexpr std::uint8_t body = 18;
constexpr std::uint8_t title = 24;
constexpr std::uint8_t weight = 36;
}  // namespace font

namespace spacing {
constexpr std::uint8_t xs = 4;
constexpr std::uint8_t sm = 8;
constexpr std::uint8_t md = 12;
constexpr std::uint8_t lg = 16;
}  // namespace spacing

constexpr std::uint16_t minimumTouchHeight = 48;
constexpr std::uint16_t preferredTouchHeight = 56;
constexpr std::uint16_t topPrinterBarHeight = 40;
constexpr std::uint16_t contentHeight = 224;
constexpr std::uint16_t bottomActionBarHeight = 56;

static_assert(topPrinterBarHeight + contentHeight + bottomActionBarHeight ==
              320);

}  // namespace filament_station::ui::design
