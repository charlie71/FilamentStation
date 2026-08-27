/**
 * @file
 * @brief Implements the OpenPrintTag decoder: NDEF/TLV/MIME-record lookup
 *        plus a small hand-rolled CBOR reader for the tag's map payload.
 */
#include "nfc/OpenPrintTag.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace filament_station {
namespace nfc {
namespace {

constexpr char kMimeType[] = "application/vnd.openprinttag";  ///< NDEF MIME type identifying an OpenPrintTag record.

/// @brief Non-owning view of a contiguous byte span.
struct ByteRange {
  const std::uint8_t* data = nullptr;  ///< Pointer to the first byte, or null if empty.
  std::size_t size = 0;                ///< Number of bytes in the span.
};

/// @brief Forward-only read cursor over a byte span, used by the CBOR reader.
struct Cursor {
  const std::uint8_t* data;   ///< Underlying buffer.
  std::size_t size;           ///< Total buffer length in bytes.
  std::size_t position = 0;   ///< Current read offset into #data.

  /// @brief Advances the cursor and returns a pointer to the consumed bytes.
  /// @param count Number of bytes to consume.
  /// @param output Out parameter receiving a pointer to the first consumed byte.
  /// @return false if fewer than `count` bytes remain.
  bool take(std::size_t count, const std::uint8_t*& output) {
    if (count > size - position) return false;
    output = data + position;
    position += count;
    return true;
  }
  /// @brief Consumes and returns a single byte.
  /// @param output Out parameter receiving the consumed byte.
  /// @return false if the cursor is already at the end.
  bool byte(std::uint8_t& output) {
    const std::uint8_t* p = nullptr;
    if (!take(1, p)) return false;
    output = *p;
    return true;
  }
};

/// @brief Reads a CBOR major-type argument (the value encoded by the
///        initial byte's low 5 bits, possibly followed by 1/2/4/8 bytes).
/// @param cursor Cursor positioned right after the initial byte.
/// @param additional The initial byte's low 5 bits ("additional information").
/// @param value Out parameter receiving the decoded argument.
/// @param indefinite Out parameter set if this is CBOR's indefinite-length marker (additional == 31).
/// @return false on truncated/malformed input.
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

/// @brief Reads one CBOR item header (major type + argument).
/// @param cursor Cursor positioned at the item's initial byte.
/// @param major Out parameter receiving the CBOR major type (0-7).
/// @param argument Out parameter receiving the decoded argument.
/// @param indefinite Out parameter set if the item uses indefinite-length encoding.
/// @return false on truncated input.
bool readHeader(Cursor& cursor, std::uint8_t& major, std::uint64_t& argument,
                bool& indefinite) {
  std::uint8_t initial = 0;
  if (!cursor.byte(initial)) return false;
  major = initial >> 5U;
  return readArgument(cursor, initial & 0x1FU, argument, indefinite);
}

/// @brief Recursively skips one CBOR item without decoding its value,
///        used to walk past map/array entries this parser does not care about.
/// @param cursor Cursor positioned at the item to skip.
/// @param depth Current recursion depth; used to reject pathologically nested input.
/// @return false on truncated/malformed input or excessive nesting (depth > 12).
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

/// @brief Reads a CBOR unsigned integer (major type 0).
/// @param cursor Cursor positioned at the item to read.
/// @param value Out parameter receiving the decoded value.
/// @return false if the item is not an unsigned integer, or on truncated input.
bool readUnsigned(Cursor& cursor, std::uint64_t& value) {
  std::uint8_t major = 0;
  bool indefinite = false;
  return readHeader(cursor, major, value, indefinite) && major == 0 &&
         !indefinite;
}

/// @brief Reads a CBOR numeric item (unsigned, negative, half/single/double float).
/// @param cursor Cursor positioned at the item to read.
/// @param value Out parameter receiving the decoded value as a double.
/// @return false if the item is not numeric, is non-finite, or on truncated input.
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

/// @brief Reads a CBOR text string (major type 3) into a fixed-size buffer.
/// @param cursor Cursor positioned at the item to read.
/// @param output Destination buffer; NUL-terminated on success.
/// @param capacity Size of `output` in bytes.
/// @return false if the item is not text, does not fit, or on truncated input.
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

/// @brief Reads a CBOR byte string (major type 2) of 3 or 4 RGB(A) bytes
///        and formats it as a "#RRGGBB" string.
/// @param cursor Cursor positioned at the item to read.
/// @param output Destination buffer receiving the formatted color string.
/// @param capacity Size of `output` in bytes.
/// @return false if the item is not a 3- or 4-byte byte string, or on truncated input.
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

/// @brief Maps an OpenPrintTag material-key integer to its short material name.
/// @param key Material class key as read from the tag's CBOR map (key 9).
/// @return Abbreviation string (e.g. "PLA"), or null if `key` is not recognized.
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

/// @brief Reads a CBOR map header (major type 5) and its pair count.
/// @param cursor Cursor positioned at the map item.
/// @param pairs Out parameter receiving the declared key/value pair count (meaningless if `indefinite`).
/// @param indefinite Out parameter set if the map uses indefinite-length encoding.
/// @return false if the item is not a map, or on truncated input.
bool parseMapHeader(Cursor& cursor, std::uint64_t& pairs, bool& indefinite) {
  std::uint8_t major = 0;
  return readHeader(cursor, major, pairs, indefinite) && major == 5;
}

/// @brief Parses the tag's outer "meta" CBOR map to locate the offset of
///        the main data map (key 0), defaulting to right after the meta
///        map if that key is absent.
/// @param payload Full CBOR payload span.
/// @param mainOffset Out parameter receiving the byte offset of the main map.
/// @return false on malformed input or an out-of-range offset.
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

/// @brief Parses the tag's main CBOR data map into a TagDefinition,
///        recognizing the material class/name/vendor/color/weight/
///        temperature keys and skipping everything else.
/// @param payload Full CBOR payload span.
/// @param offset Byte offset of the main map, as found by #parseMeta().
/// @param definition Out parameter receiving the decoded fields.
/// @return false on malformed input, or if the required material-class key (8) is missing.
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

/// @brief Walks the NDEF TLV/record structure to find the OpenPrintTag MIME record.
/// @param data Raw NDEF message bytes.
/// @param size Length of `data` in bytes.
/// @param payload Out parameter receiving the matching MIME record's payload span.
/// @return true if a record with #kMimeType was found.
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
