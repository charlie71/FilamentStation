#include "services/Ntag21x.h"

namespace filament_station {
namespace services {

models::TagTechnology identifyNtag21x(const std::uint8_t* version,
                                      std::size_t versionLength,
                                      const std::uint8_t* capability) {
  if (version == nullptr || capability == nullptr || versionLength != 8 ||
      version[0] != 0x00 || version[1] != 0x04 || version[2] != 0x04 ||
      version[3] != 0x02 || version[7] != 0x03 || capability[0] != 0xE1) {
    return models::TagTechnology::OtherIso14443A;
  }
  if (version[6] == 0x0F && capability[2] == 0x12)
    return models::TagTechnology::Ntag213;
  if (version[6] == 0x11 && capability[2] == 0x3E)
    return models::TagTechnology::Ntag215;
  if (version[6] == 0x13 && capability[2] == 0x6D)
    return models::TagTechnology::Ntag216;
  return models::TagTechnology::OtherIso14443A;
}

bool ntag21xRangeWritable(models::TagTechnology technology,
                          std::uint8_t lastPage,
                          const std::uint8_t* capability,
                          const std::uint8_t* staticLocks,
                          const std::uint8_t* dynamicLocks,
                          std::uint8_t auth0) {
  if (technology != models::TagTechnology::Ntag213 &&
      technology != models::TagTechnology::Ntag215 &&
      technology != models::TagTechnology::Ntag216)
    return false;
  if (capability == nullptr || staticLocks == nullptr ||
      dynamicLocks == nullptr || capability[0] != 0xE1 ||
      (capability[3] & 0x0FU) == 0x0FU)
    return false;
  if (staticLocks[0] != 0 || staticLocks[1] != 0 ||
      dynamicLocks[0] != 0 || dynamicLocks[1] != 0 || dynamicLocks[2] != 0)
    return false;
  return auth0 == 0xFF || auth0 > lastPage;
}

}  // namespace services
}  // namespace filament_station
