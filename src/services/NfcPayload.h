/**
 * @file
 * @brief Parses and builds the Type-2 NDEF TLV payload used to identify a
 *        tag's format (FilamentStation "spoolman:&lt;id&gt;", legacy
 *        "spool:&lt;id&gt;", or a Bambu marker) from raw page data.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace services {

/// @brief Which recognized payload (if any) an NDEF Type-2 TLV area holds.
enum class NfcPayloadType : std::uint8_t {
  Empty,     ///< No NDEF message, or an empty one.
  Spoolman,  ///< Native FilamentStation "spoolman:&lt;id&gt;" text record.
  Bambu,     ///< NDEF text record containing a "bambu" marker.
  Legacy,    ///< Legacy plain-text "spool:&lt;id&gt;" text record.
  Unknown,   ///< A well-formed NDEF message this parser does not recognize.
  Invalid,   ///< Malformed TLV/NDEF structure.
};

/// @brief Result of parsing a Type-2 NDEF TLV area.
struct NfcPayloadInfo {
  NfcPayloadType type = NfcPayloadType::Invalid;  ///< Classified payload type.
  std::uint32_t spoolId = 0;  ///< Decoded spool id, for NfcPayloadType::Spoolman/NfcPayloadType::Legacy.
};

/// @brief Parses the Type-2 TLV area beginning at page 4.
/// @param data Raw page data starting at page 4.
/// @param size Length of `data` in bytes.
/// @return Classified payload info; NfcPayloadType::Invalid on malformed input.
NfcPayloadInfo parseType2Ndef(const std::uint8_t* data, std::size_t size);

/// @brief Builds a complete Type-2 NDEF TLV containing the text "spoolman:&lt;id&gt;".
/// @param spoolId Spool id to encode; must be non-zero.
/// @param output Destination buffer, page-aligned; writable starting at page 4.
/// @param capacity Size of `output` in bytes.
/// @param outputSize Out parameter receiving the number of bytes written.
// Builds a complete Type-2 NDEF TLV containing the text "spoolman:<id>".
// The output can be written page-wise beginning at page 4.
/// @return false if `spoolId` is 0, or the encoded record does not fit `capacity`.
bool buildSpoolmanType2Ndef(std::uint32_t spoolId, std::uint8_t* output,
                           std::size_t capacity, std::size_t& outputSize);

}  // namespace services
}  // namespace filament_station
