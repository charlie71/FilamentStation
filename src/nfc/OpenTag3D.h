/**
 * @file
 * @brief Decoder for the third-party OpenTag3D MIME NDEF format.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "models/TagDefinition.h"

namespace filament_station {
namespace nfc {

/// @brief Checks whether an NDEF message contains an OpenTag3D MIME record.
/// @param data Raw NDEF message bytes.
/// @param size Length of `data` in bytes.
/// @return true if a matching MIME record is present.
bool containsOpenTag3DMimeRecord(const std::uint8_t* data, std::size_t size);
/// @brief Decodes an OpenTag3D MIME NDEF message.
/// @param data Raw NDEF message bytes.
/// @param size Length of `data` in bytes.
/// @param definition Out parameter receiving the decoded fields on success.
/// @return true if a valid OpenTag3D record was found and decoded.
bool parseOpenTag3DNdef(const std::uint8_t* data, std::size_t size,
                        models::TagDefinition& definition);

}  // namespace nfc
}  // namespace filament_station
