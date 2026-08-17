#include "services/NfcPayload.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace filament_station {
namespace services {
namespace {

bool parseUnsigned(const char* begin, const char* end, std::uint32_t& value) {
  if (begin == end) return false;
  std::uint64_t result = 0;
  for (const char* p = begin; p != end; ++p) {
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    result = result * 10U + static_cast<unsigned>(*p - '0');
    if (result > UINT32_MAX) return false;
  }
  value = static_cast<std::uint32_t>(result);
  return true;
}

bool containsIgnoreCase(const char* text, std::size_t length,
                        const char* needle) {
  const std::size_t needleLength = std::strlen(needle);
  if (needleLength == 0 || needleLength > length) return false;
  for (std::size_t i = 0; i + needleLength <= length; ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needleLength; ++j) {
      if (std::tolower(static_cast<unsigned char>(text[i + j])) !=
          std::tolower(static_cast<unsigned char>(needle[j]))) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

NfcPayloadInfo classifyText(const char* text, std::size_t length) {
  constexpr char kSpoolmanPrefix[] = "spoolman:";
  constexpr char kLegacyPrefix[] = "spool:";
  std::uint32_t spoolId = 0;
  if (length > sizeof(kSpoolmanPrefix) - 1 &&
      std::memcmp(text, kSpoolmanPrefix, sizeof(kSpoolmanPrefix) - 1) == 0) {
    if (parseUnsigned(text + sizeof(kSpoolmanPrefix) - 1, text + length,
                      spoolId) && spoolId != 0) {
      return {NfcPayloadType::Spoolman, spoolId};
    }
    return {NfcPayloadType::Invalid, 0};
  }
  if (length > sizeof(kLegacyPrefix) - 1 &&
      std::memcmp(text, kLegacyPrefix, sizeof(kLegacyPrefix) - 1) == 0 &&
      parseUnsigned(text + sizeof(kLegacyPrefix) - 1, text + length, spoolId) &&
      spoolId != 0) {
    return {NfcPayloadType::Legacy, spoolId};
  }
  if (containsIgnoreCase(text, length, "bambu")) {
    return {NfcPayloadType::Bambu, 0};
  }
  return {NfcPayloadType::Unknown, 0};
}

}  // namespace

NfcPayloadInfo parseType2Ndef(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) return {NfcPayloadType::Invalid, 0};
  std::size_t position = 0;
  while (position < size) {
    const std::uint8_t type = data[position++];
    if (type == 0x00) continue;
    if (type == 0xFE) return {NfcPayloadType::Empty, 0};
    if (position >= size) return {NfcPayloadType::Invalid, 0};
    std::size_t length = data[position++];
    if (length == 0xFF) {
      if (position + 2 > size) return {NfcPayloadType::Invalid, 0};
      length = (static_cast<std::size_t>(data[position]) << 8U) |
               data[position + 1];
      position += 2;
    }
    if (position + length > size) return {NfcPayloadType::Invalid, 0};
    if (type != 0x03) {
      position += length;
      continue;
    }
    if (length == 0) return {NfcPayloadType::Empty, 0};
    const std::uint8_t* record = data + position;
    // A standards-compliant empty NDEF message is commonly encoded as one
    // short record with MB=1, ME=1, SR=1, TNF=Empty, zero type length and
    // zero payload length: D0 00 00. NFC utility apps use this representation
    // when formatting an otherwise empty tag.
    if (length == 3 && record[0] == 0xD0 && record[1] == 0x00 &&
        record[2] == 0x00) {
      return {NfcPayloadType::Empty, 0};
    }
    if (length < 5 || (record[0] & 0x10U) == 0 || record[1] != 1 ||
        record[3] != 'T') {
      return {NfcPayloadType::Unknown, 0};
    }
    const std::size_t payloadLength = record[2];
    if (4 + payloadLength > length || payloadLength < 1) {
      return {NfcPayloadType::Invalid, 0};
    }
    const std::uint8_t languageLength = record[4] & 0x3FU;
    if (payloadLength < 1U + languageLength) {
      return {NfcPayloadType::Invalid, 0};
    }
    const char* text = reinterpret_cast<const char*>(record + 5 + languageLength);
    return classifyText(text, payloadLength - 1U - languageLength);
  }
  return {NfcPayloadType::Empty, 0};
}

bool buildSpoolmanType2Ndef(std::uint32_t spoolId, std::uint8_t* output,
                           std::size_t capacity, std::size_t& outputSize) {
  outputSize = 0;
  if (output == nullptr || spoolId == 0) return false;
  char text[32]{};
  const int textLength = std::snprintf(text, sizeof(text), "spoolman:%lu",
                                       static_cast<unsigned long>(spoolId));
  if (textLength <= 0 || static_cast<std::size_t>(textLength) >= sizeof(text)) {
    return false;
  }
  const std::size_t payloadLength = 3U + static_cast<std::size_t>(textLength);
  const std::size_t recordLength = 4U + payloadLength;
  const std::size_t rawLength = 2U + recordLength + 1U;
  const std::size_t paddedLength = (rawLength + 3U) & ~std::size_t{3U};
  if (recordLength > 254 || paddedLength > capacity) return false;
  std::memset(output, 0, paddedLength);
  std::size_t p = 0;
  output[p++] = 0x03;
  output[p++] = static_cast<std::uint8_t>(recordLength);
  output[p++] = 0xD1;  // MB, ME, short record, well-known type
  output[p++] = 0x01;
  output[p++] = static_cast<std::uint8_t>(payloadLength);
  output[p++] = 'T';
  output[p++] = 0x02;
  output[p++] = 'e';
  output[p++] = 'n';
  std::memcpy(output + p, text, static_cast<std::size_t>(textLength));
  p += static_cast<std::size_t>(textLength);
  output[p++] = 0xFE;
  outputSize = paddedLength;
  return true;
}

}  // namespace services
}  // namespace filament_station
