/**
 * @file
 * @brief Implements services::extractHexSha256().
 */
#include "services/Sha256Hex.h"

#include <cctype>
#include <cstddef>

namespace filament_station {
namespace services {

bool extractHexSha256(const char* text, char* out) {
  if (text == nullptr) return false;
  std::size_t index = 0;
  for (; index < 64; ++index) {
    const unsigned char c = static_cast<unsigned char>(text[index]);
    if (c == '\0' || std::isxdigit(c) == 0) return false;
    out[index] = static_cast<char>(std::tolower(c));
  }
  out[64] = '\0';
  return true;
}

}  // namespace services
}  // namespace filament_station
