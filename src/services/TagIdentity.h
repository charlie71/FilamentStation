#pragma once

#include <cstddef>
#include <cstdint>

#include "models/TagIdentity.h"

namespace filament_station {
namespace services {

bool normalizeTagIdentity(const char* input,
                          models::TagIdentitySource source,
                          models::TagIdentity& identity);

bool tagIdentityFromUid(const std::uint8_t* uid, std::size_t uidLength,
                        models::TagIdentity& identity);

bool tagIdentityFromBambuUuid(const char* uuid,
                              models::TagIdentity& identity);

}  // namespace services
}  // namespace filament_station
