/**
 * @file
 * @brief Implements services::parseSemVer()/compareSemVer().
 */
#include "services/SemVer.h"

#include <cstdlib>

namespace filament_station {
namespace services {
namespace {

/// @brief Parses one dot-separated unsigned numeric component.
/// @param text Start of the component to parse.
/// @param end Out parameter receiving a pointer just past the parsed digits.
/// @param out Out parameter receiving the parsed value.
/// @return false if `text` does not start with a digit, or the value is negative.
bool parseComponent(const char* text, char** end, std::uint32_t& out) {
  if (text == nullptr || *text < '0' || *text > '9') return false;
  const long value = std::strtol(text, end, 10);
  if (*end == text || value < 0) return false;
  out = static_cast<std::uint32_t>(value);
  return true;
}

}  // namespace

bool parseSemVer(const char* text, SemVer& out) {
  if (text == nullptr) return false;
  if (*text == 'v' || *text == 'V') ++text;

  char* end = nullptr;
  SemVer parsed{};
  if (!parseComponent(text, &end, parsed.major) || *end != '.') return false;
  text = end + 1;
  if (!parseComponent(text, &end, parsed.minor) || *end != '.') return false;
  text = end + 1;
  if (!parseComponent(text, &end, parsed.patch)) return false;

  parsed.hasSuffix = *end == '-' || *end == '+';
  out = parsed;
  return true;
}

int compareSemVer(const SemVer& a, const SemVer& b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  if (a.hasSuffix != b.hasSuffix) return a.hasSuffix ? -1 : 1;
  return 0;
}

}  // namespace services
}  // namespace filament_station
