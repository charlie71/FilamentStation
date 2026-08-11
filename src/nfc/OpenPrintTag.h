#pragma once

#include <cstddef>
#include <cstdint>

#include "models/TagDefinition.h"

namespace filament_station {
namespace nfc {

bool containsOpenPrintTagMimeRecord(const std::uint8_t* data,
                                    std::size_t size);
bool parseOpenPrintTagNdef(const std::uint8_t* data, std::size_t size,
                           models::TagDefinition& definition);

}  // namespace nfc
}  // namespace filament_station
