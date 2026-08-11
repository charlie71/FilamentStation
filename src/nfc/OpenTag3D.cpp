#include "nfc/OpenTag3D.h"

#include <cstdio>
#include <cstring>

namespace filament_station {
namespace nfc {
namespace {

constexpr char kMimeType[] = "application/opentag3d";
constexpr std::size_t kCoreMinimumSize = 0x66;
constexpr std::size_t kExtendedSize = 0xBB;

struct ByteRange {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;
};

bool findMimePayload(const std::uint8_t* data, std::size_t size,
                     ByteRange& payload) {
  if (data == nullptr || size < 3) return false;
  std::size_t position = size >= 4 && data[0] == 0xE1 ? 4 : 0;
  while (position < size) {
    const std::uint8_t tlvType = data[position++];
    if (tlvType == 0x00) continue;
    if (tlvType == 0xFE || position >= size) return false;
    std::size_t tlvLength = data[position++];
    if (tlvLength == 0xFF) {
      if (position + 2 > size) return false;
      tlvLength = (static_cast<std::size_t>(data[position]) << 8U) |
                  data[position + 1];
      position += 2;
    }
    if (tlvLength > size - position) return false;
    if (tlvType != 0x03) {
      position += tlvLength;
      continue;
    }
    const std::size_t end = position + tlvLength;
    while (position < end) {
      const std::uint8_t header = data[position++];
      const bool shortRecord = (header & 0x10U) != 0;
      const bool hasId = (header & 0x08U) != 0;
      if ((header & 0x20U) != 0 || position >= end) return false;
      const std::uint8_t typeLength = data[position++];
      std::size_t payloadLength = 0;
      if (shortRecord) {
        if (position >= end) return false;
        payloadLength = data[position++];
      } else {
        if (position + 4 > end) return false;
        for (unsigned index = 0; index < 4; ++index)
          payloadLength = (payloadLength << 8U) | data[position++];
      }
      std::size_t idLength = 0;
      if (hasId) {
        if (position >= end) return false;
        idLength = data[position++];
      }
      if (typeLength + idLength > end - position) return false;
      const std::uint8_t* type = data + position;
      position += typeLength + idLength;
      if (payloadLength > end - position) return false;
      const bool mime = (header & 0x07U) == 0x02U &&
                        typeLength == sizeof(kMimeType) - 1U &&
                        std::memcmp(type, kMimeType,
                                    sizeof(kMimeType) - 1U) == 0;
      if (mime) {
        payload = {data + position, payloadLength};
        return true;
      }
      position += payloadLength;
    }
    return false;
  }
  return false;
}

std::uint16_t readBigEndian16(const std::uint8_t* data) {
  return (static_cast<std::uint16_t>(data[0]) << 8U) | data[1];
}

bool copyFixedText(const ByteRange& payload, std::size_t offset,
                   std::size_t length, char* output, std::size_t capacity) {
  if (offset + length > payload.size || capacity == 0) return false;
  std::size_t used = 0;
  while (used < length && payload.data[offset + used] != 0 &&
         used + 1 < capacity) {
    output[used] = static_cast<char>(payload.data[offset + used]);
    ++used;
  }
  while (used > 0 && output[used - 1] == ' ') --used;
  output[used] = '\0';
  return true;
}

}  // namespace

bool containsOpenTag3DMimeRecord(const std::uint8_t* data, std::size_t size) {
  ByteRange payload{};
  return findMimePayload(data, size, payload);
}

bool parseOpenTag3DNdef(const std::uint8_t* data, std::size_t size,
                        models::TagDefinition& definition) {
  ByteRange payload{};
  if (!findMimePayload(data, size, payload) ||
      payload.size < kCoreMinimumSize) return false;

  const std::uint16_t version = readBigEndian16(payload.data);
  if (version < 1000U || version >= 2000U) return false;

  models::TagDefinition parsed{};
  parsed.format = models::TagFormat::OpenTag3D;
  char modifier[6]{};
  if (!copyFixedText(payload, 0x02, 5, parsed.material,
                     sizeof(parsed.material)) ||
      !copyFixedText(payload, 0x07, 5, modifier, sizeof(modifier)) ||
      !copyFixedText(payload, 0x1B, 16, parsed.vendor,
                     sizeof(parsed.vendor)) ||
      !copyFixedText(payload, 0x2B, 32, parsed.colorName,
                     sizeof(parsed.colorName)) ||
      parsed.material[0] == '\0' || parsed.vendor[0] == '\0') return false;

  if (modifier[0] != '\0')
    std::snprintf(parsed.filamentName, sizeof(parsed.filamentName), "%s %s",
                  parsed.material, modifier);
  else
    std::snprintf(parsed.filamentName, sizeof(parsed.filamentName), "%s",
                  parsed.material);

  const std::uint8_t* color = payload.data + 0x4B;
  if (color[3] == 0) return false;
  std::snprintf(parsed.colorCode, sizeof(parsed.colorCode), "#%02X%02X%02X",
                color[0], color[1], color[2]);

  const std::uint16_t targetWeight = readBigEndian16(payload.data + 0x5E);
  const std::int16_t printTemperature =
      static_cast<std::int16_t>(payload.data[0x60] * 5U);
  if (targetWeight == 0 || printTemperature == 0 ||
      printTemperature > 500) return false;
  parsed.nominalFilamentWeightG = static_cast<float>(targetWeight);
  parsed.nozzleTempMinC = printTemperature;
  parsed.nozzleTempMaxC = printTemperature;

  if (payload.size >= kExtendedSize) {
    parsed.emptySpoolWeightG =
        static_cast<float>(readBigEndian16(payload.data + 0xAC));
    const std::int16_t minimum =
        static_cast<std::int16_t>(payload.data[0xB4] * 5U);
    const std::int16_t maximum =
        static_cast<std::int16_t>(payload.data[0xB5] * 5U);
    if (minimum != 0 || maximum != 0) {
      if (minimum == 0 || maximum == 0 || minimum > maximum || maximum > 500)
        return false;
      parsed.nozzleTempMinC = minimum;
      parsed.nozzleTempMaxC = maximum;
    }
  }

  std::snprintf(parsed.sourceDescription, sizeof(parsed.sourceDescription),
                "OpenTag3D v%u.%03u MIME NDEF", version / 1000U,
                version % 1000U);
  definition = parsed;
  return true;
}

}  // namespace nfc
}  // namespace filament_station
