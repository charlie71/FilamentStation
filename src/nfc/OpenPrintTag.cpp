#include "nfc/OpenPrintTag.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace filament_station {
namespace nfc {
namespace {

constexpr char kMimeType[] = "application/vnd.openprinttag";

struct ByteRange {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;
};

struct Cursor {
  const std::uint8_t* data;
  std::size_t size;
  std::size_t position = 0;

  bool take(std::size_t count, const std::uint8_t*& output) {
    if (count > size - position) return false;
    output = data + position;
    position += count;
    return true;
  }
  bool byte(std::uint8_t& output) {
    const std::uint8_t* p = nullptr;
    if (!take(1, p)) return false;
    output = *p;
    return true;
  }
};

bool readArgument(Cursor& cursor, std::uint8_t additional,
                  std::uint64_t& value, bool& indefinite) {
  indefinite = false;
  if (additional < 24) {
    value = additional;
    return true;
  }
  if (additional == 31) {
    indefinite = true;
    value = 0;
    return true;
  }
  const std::size_t count = additional == 24 ? 1 : additional == 25 ? 2 :
                            additional == 26 ? 4 : additional == 27 ? 8 : 0;
  const std::uint8_t* bytes = nullptr;
  if (count == 0 || !cursor.take(count, bytes)) return false;
  value = 0;
  for (std::size_t index = 0; index < count; ++index)
    value = (value << 8U) | bytes[index];
  return true;
}

bool readHeader(Cursor& cursor, std::uint8_t& major, std::uint64_t& argument,
                bool& indefinite) {
  std::uint8_t initial = 0;
  if (!cursor.byte(initial)) return false;
  major = initial >> 5U;
  return readArgument(cursor, initial & 0x1FU, argument, indefinite);
}

bool skipValue(Cursor& cursor, unsigned depth = 0) {
  if (depth > 12) return false;
  std::uint8_t major = 0;
  std::uint64_t argument = 0;
  bool indefinite = false;
  if (!readHeader(cursor, major, argument, indefinite)) return false;
  if (major == 0 || major == 1 || major == 7) return !indefinite;
  if (major == 2 || major == 3) {
    if (!indefinite) {
      const std::uint8_t* ignored = nullptr;
      return argument <= SIZE_MAX &&
             cursor.take(static_cast<std::size_t>(argument), ignored);
    }
    while (cursor.position < cursor.size && cursor.data[cursor.position] != 0xFF)
      if (!skipValue(cursor, depth + 1)) return false;
    return cursor.position < cursor.size && ++cursor.position > 0;
  }
  if (major == 4 || major == 5) {
    std::uint64_t remaining = indefinite ? UINT64_MAX : argument * (major == 5 ? 2U : 1U);
    while ((indefinite && cursor.position < cursor.size &&
            cursor.data[cursor.position] != 0xFF) ||
           (!indefinite && remaining > 0)) {
      if (!skipValue(cursor, depth + 1)) return false;
      if (!indefinite) --remaining;
    }
    if (indefinite) {
      if (cursor.position >= cursor.size) return false;
      ++cursor.position;
    }
    return true;
  }
  return major == 6 && !indefinite && skipValue(cursor, depth + 1);
}

bool readUnsigned(Cursor& cursor, std::uint64_t& value) {
  std::uint8_t major = 0;
  bool indefinite = false;
  return readHeader(cursor, major, value, indefinite) && major == 0 &&
         !indefinite;
}

bool readNumber(Cursor& cursor, double& value) {
  const std::size_t start = cursor.position;
  std::uint8_t major = 0;
  std::uint64_t argument = 0;
  bool indefinite = false;
  if (!readHeader(cursor, major, argument, indefinite) || indefinite) return false;
  if (major == 0) {
    value = static_cast<double>(argument);
    return true;
  }
  if (major == 1) {
    value = -1.0 - static_cast<double>(argument);
    return true;
  }
  if (major != 7) return false;
  const std::uint8_t additional = cursor.data[start] & 0x1FU;
  if (additional == 25) {
    const std::uint16_t half = static_cast<std::uint16_t>(argument);
    const int sign = (half & 0x8000U) ? -1 : 1;
    const int exponent = (half >> 10U) & 0x1FU;
    const int fraction = half & 0x03FFU;
    if (exponent == 0)
      value = sign * std::ldexp(static_cast<double>(fraction), -24);
    else if (exponent == 31)
      return false;
    else
      value = sign * std::ldexp(static_cast<double>(fraction + 1024),
                                exponent - 25);
    return true;
  }
  if (additional == 26) {
    const std::uint32_t bits = static_cast<std::uint32_t>(argument);
    float number = 0.0F;
    std::memcpy(&number, &bits, sizeof(number));
    value = number;
    return std::isfinite(value);
  }
  if (additional == 27) {
    std::uint64_t bits = argument;
    std::memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value);
  }
  return false;
}

bool readText(Cursor& cursor, char* output, std::size_t capacity) {
  std::uint8_t major = 0;
  std::uint64_t length = 0;
  bool indefinite = false;
  if (!readHeader(cursor, major, length, indefinite) || major != 3 ||
      indefinite || length >= capacity || length > SIZE_MAX) return false;
  const std::uint8_t* bytes = nullptr;
  if (!cursor.take(static_cast<std::size_t>(length), bytes)) return false;
  std::memcpy(output, bytes, static_cast<std::size_t>(length));
  output[length] = '\0';
  return true;
}

bool readColor(Cursor& cursor, char* output, std::size_t capacity) {
  std::uint8_t major = 0;
  std::uint64_t length = 0;
  bool indefinite = false;
  if (!readHeader(cursor, major, length, indefinite) || major != 2 ||
      indefinite || (length != 3 && length != 4)) return false;
  const std::uint8_t* bytes = nullptr;
  if (!cursor.take(static_cast<std::size_t>(length), bytes)) return false;
  std::snprintf(output, capacity, "#%02X%02X%02X", bytes[0], bytes[1], bytes[2]);
  return true;
}

const char* materialAbbreviation(std::uint64_t key) {
  static const char* const values[] = {
      "PLA", "PETG", "TPU", "ABS", "ASA", "PC", "PCTG", "PP",
      "PA6", "PA11", "PA12", "PA66", "CPE", "TPE", "HIPS", "PHA",
      "PET", "PEI", "PBT", "PVB", "PVA", "PEKK", "PEEK", "BVOH",
      "TPC", "PPS", "PPSU", "PVC", "PEBA", "PVDF", "PPA"};
  if (key < sizeof(values) / sizeof(values[0])) return values[key];
  if (key == 42) return "PA612";
  return nullptr;
}

bool parseMapHeader(Cursor& cursor, std::uint64_t& pairs, bool& indefinite) {
  std::uint8_t major = 0;
  return readHeader(cursor, major, pairs, indefinite) && major == 5;
}

bool parseMeta(const ByteRange& payload, std::size_t& mainOffset) {
  Cursor cursor{payload.data, payload.size};
  std::uint64_t pairs = 0;
  bool indefinite = false;
  if (!parseMapHeader(cursor, pairs, indefinite)) return false;
  std::uint64_t processed = 0;
  bool explicitMainOffset = false;
  while ((indefinite && cursor.position < cursor.size &&
          cursor.data[cursor.position] != 0xFF) ||
         (!indefinite && processed < pairs)) {
    std::uint64_t key = 0;
    if (!readUnsigned(cursor, key)) return false;
    if (key == 0) {
      std::uint64_t offset = 0;
      if (!readUnsigned(cursor, offset) || offset >= payload.size) return false;
      mainOffset = static_cast<std::size_t>(offset);
      explicitMainOffset = true;
    } else if (!skipValue(cursor)) {
      return false;
    }
    ++processed;
  }
  if (indefinite) {
    if (cursor.position >= cursor.size) return false;
    ++cursor.position;
  }
  if (!explicitMainOffset) mainOffset = cursor.position;
  return mainOffset < payload.size;
}

bool parseMain(const ByteRange& payload, std::size_t offset,
               models::TagDefinition& definition) {
  Cursor cursor{payload.data + offset, payload.size - offset};
  std::uint64_t pairs = 0;
  bool indefinite = false;
  if (!parseMapHeader(cursor, pairs, indefinite)) return false;
  std::uint64_t processed = 0;
  bool materialClassPresent = false;
  bool nominalWeightPresent = false;
  while ((indefinite && cursor.position < cursor.size &&
          cursor.data[cursor.position] != 0xFF) ||
         (!indefinite && processed < pairs)) {
    std::uint64_t key = 0;
    if (!readUnsigned(cursor, key)) return false;
    bool consumed = true;
    if (key == 8) {
      std::uint64_t value = 0;
      consumed = readUnsigned(cursor, value);
      materialClassPresent = consumed;
    } else if (key == 9) {
      std::uint64_t value = 0;
      consumed = readUnsigned(cursor, value);
      const char* abbreviation = consumed ? materialAbbreviation(value) : nullptr;
      if (abbreviation != nullptr)
        std::snprintf(definition.material, sizeof(definition.material), "%s",
                      abbreviation);
    } else if (key == 10) {
      consumed = readText(cursor, definition.filamentName,
                          sizeof(definition.filamentName));
    } else if (key == 11) {
      consumed = readText(cursor, definition.vendor, sizeof(definition.vendor));
    } else if (key == 16 || key == 17 || key == 18) {
      double value = 0.0;
      consumed = readNumber(cursor, value);
      if (consumed && value >= 0.0 && value <= 100000.0) {
        if (key == 16 && !nominalWeightPresent) {
          definition.nominalFilamentWeightG = static_cast<float>(value);
          nominalWeightPresent = true;
        } else if (key == 17) {
          definition.nominalFilamentWeightG = static_cast<float>(value);
          nominalWeightPresent = true;
        } else if (key == 18) {
          definition.emptySpoolWeightG = static_cast<float>(value);
        }
      }
    } else if (key == 19) {
      consumed = readColor(cursor, definition.colorCode,
                           sizeof(definition.colorCode));
    } else if (key == 34 || key == 35) {
      double value = 0.0;
      consumed = readNumber(cursor, value);
      if (consumed && value >= 0.0 && value <= 500.0) {
        if (key == 34) definition.nozzleTempMinC = static_cast<std::int16_t>(value);
        if (key == 35) definition.nozzleTempMaxC = static_cast<std::int16_t>(value);
      }
    } else {
      consumed = skipValue(cursor);
    }
    if (!consumed) return false;
    ++processed;
  }
  if (indefinite) {
    if (cursor.position >= cursor.size || cursor.data[cursor.position] != 0xFF)
      return false;
    ++cursor.position;
  }
  return materialClassPresent;
}

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
                        std::memcmp(type, kMimeType, sizeof(kMimeType) - 1U) == 0;
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

}  // namespace

bool containsOpenPrintTagMimeRecord(const std::uint8_t* data,
                                    std::size_t size) {
  ByteRange payload{};
  return findMimePayload(data, size, payload);
}

bool parseOpenPrintTagNdef(const std::uint8_t* data, std::size_t size,
                           models::TagDefinition& definition) {
  ByteRange payload{};
  if (!findMimePayload(data, size, payload)) return false;
  std::size_t mainOffset = 0;
  models::TagDefinition parsed{};
  parsed.format = models::TagFormat::OpenPrintTag;
  if (!parseMeta(payload, mainOffset) ||
      !parseMain(payload, mainOffset, parsed)) return false;
  if (parsed.nozzleTempMinC > parsed.nozzleTempMaxC &&
      parsed.nozzleTempMaxC != 0) return false;
  std::snprintf(parsed.sourceDescription, sizeof(parsed.sourceDescription),
                "OpenPrintTag MIME NDEF/CBOR");
  definition = parsed;
  return true;
}

}  // namespace nfc
}  // namespace filament_station
