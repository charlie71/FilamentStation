#include "services/TagIdentity.h"

#include <cctype>
#include <cstring>

namespace filament_station {
namespace services {

bool normalizeTagIdentity(const char* input,
                          models::TagIdentitySource source,
                          models::TagIdentity& identity) {
  identity = {};
  if (input == nullptr || source == models::TagIdentitySource::Unknown)
    return false;

  std::size_t length = 0;
  for (const unsigned char* cursor =
           reinterpret_cast<const unsigned char*>(input);
       *cursor != '\0'; ++cursor) {
    if (*cursor == ':' || *cursor == '-' || std::isspace(*cursor)) continue;
    if (!std::isxdigit(*cursor) || length + 1 >= sizeof(identity.value)) {
      identity = {};
      return false;
    }
    identity.value[length++] =
        static_cast<char>(std::toupper(static_cast<unsigned char>(*cursor)));
  }

  if (length == 0 || (length % 2U) != 0U) {
    identity = {};
    return false;
  }
  identity.value[length] = '\0';
  identity.source = source;
  return true;
}

bool tagIdentityFromUid(const std::uint8_t* uid, std::size_t uidLength,
                        models::TagIdentity& identity) {
  identity = {};
  if (uid == nullptr || uidLength == 0 || uidLength * 2 >= sizeof(identity.value))
    return false;

  constexpr char digits[] = "0123456789ABCDEF";
  for (std::size_t index = 0; index < uidLength; ++index) {
    identity.value[index * 2] = digits[uid[index] >> 4U];
    identity.value[index * 2 + 1] = digits[uid[index] & 0x0FU];
  }
  identity.value[uidLength * 2] = '\0';
  identity.source = models::TagIdentitySource::NfcUid;
  return true;
}

bool tagIdentityFromBambuUuid(const char* uuid,
                              models::TagIdentity& identity) {
  if (!normalizeTagIdentity(uuid, models::TagIdentitySource::BambuUuid,
                            identity))
    return false;
  if (std::strlen(identity.value) != 32U) {
    identity = {};
    return false;
  }
  return true;
}

}  // namespace services
}  // namespace filament_station
