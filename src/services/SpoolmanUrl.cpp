/**
 * @file
 * @brief Implements services::buildNormalizedSpoolmanUrl()/parseNormalizedSpoolmanUrl().
 */
#include "services/SpoolmanUrl.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace filament_station {
namespace services {
namespace {
/// @brief Validates a hostname/IP: non-empty, no spaces, no path or port separators.
/// @param host NUL-terminated host string to validate.
/// @return true if `host` is a plausible bare hostname.
bool validHost(const char* host) {
  if (host == nullptr || host[0] == '\0' || std::strchr(host, ' ') != nullptr)
    return false;
  return std::strchr(host, '/') == nullptr && std::strchr(host, ':') == nullptr;
}

/// @brief Validates a port string: all digits, in range 1-65535.
/// @param port NUL-terminated port string to validate.
/// @return true if `port` is a valid port number.
bool validPort(const char* port) {
  if (port == nullptr || port[0] == '\0') return false;
  unsigned value = 0;
  for (const char* cursor = port; *cursor != '\0'; ++cursor) {
    if (!std::isdigit(static_cast<unsigned char>(*cursor))) return false;
    value = value * 10U + static_cast<unsigned>(*cursor - '0');
    if (value > 65535U) return false;
  }
  return value > 0;
}
}  // namespace

bool buildNormalizedSpoolmanUrl(const SpoolmanUrlParts& parts, char* output,
                                std::size_t outputCapacity) {
  if (output == nullptr || outputCapacity == 0 ||
      (std::strcmp(parts.protocol, "http") != 0 &&
       std::strcmp(parts.protocol, "https") != 0) ||
      !validHost(parts.host) || !validPort(parts.port) ||
      parts.basePath[0] != '/')
    return false;

  char path[sizeof(parts.basePath)]{};
  std::snprintf(path, sizeof(path), "%s", parts.basePath);
  std::size_t length = std::strlen(path);
  while (length > 1 && path[length - 1] == '/') path[--length] = '\0';
  const int written = std::snprintf(output, outputCapacity, "%s://%s:%s%s",
                                    parts.protocol, parts.host, parts.port, path);
  return written > 0 && static_cast<std::size_t>(written) < outputCapacity;
}

bool parseNormalizedSpoolmanUrl(const char* url, SpoolmanUrlParts& parts) {
  if (url == nullptr) return false;
  const char* separator = std::strstr(url, "://");
  if (separator == nullptr) return false;
  const std::size_t protocolLength = static_cast<std::size_t>(separator - url);
  if (protocolLength == 0 || protocolLength >= sizeof(parts.protocol)) return false;
  std::memcpy(parts.protocol, url, protocolLength);
  parts.protocol[protocolLength] = '\0';
  if (std::strcmp(parts.protocol, "http") != 0 &&
      std::strcmp(parts.protocol, "https") != 0)
    return false;

  const char* authority = separator + 3;
  const char* path = std::strchr(authority, '/');
  const char* colon = std::strchr(authority, ':');
  if (colon == nullptr || (path != nullptr && colon > path)) return false;
  const std::size_t hostLength = static_cast<std::size_t>(colon - authority);
  const char* portEnd = path != nullptr ? path : url + std::strlen(url);
  const std::size_t portLength = static_cast<std::size_t>(portEnd - colon - 1);
  if (hostLength == 0 || hostLength >= sizeof(parts.host) || portLength == 0 ||
      portLength >= sizeof(parts.port))
    return false;
  std::memcpy(parts.host, authority, hostLength);
  parts.host[hostLength] = '\0';
  std::memcpy(parts.port, colon + 1, portLength);
  parts.port[portLength] = '\0';
  std::snprintf(parts.basePath, sizeof(parts.basePath), "%s",
                path != nullptr ? path : "/api/v1");
  return validHost(parts.host) && validPort(parts.port) &&
         parts.basePath[0] == '/';
}

}  // namespace services
}  // namespace filament_station
