#pragma once

#include <cstdint>

namespace filament_station::models {

struct NetworkSettings {
  char hostname[33]{};
  bool dhcp = true;
  char ipAddress[16]{};
  char gateway[16]{};
  char subnetMask[16]{};
  char dns[16]{};
  char portalName[33]{};
  std::uint16_t portalTimeoutSeconds = 180;
  std::uint16_t connectTimeoutSeconds = 20;
};

}  // namespace filament_station::models
