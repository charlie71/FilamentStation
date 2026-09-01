/**
 * @file
 * @brief Raw NFC read data and the parsed, capability-annotated result
 *        produced by nfc::TagParserRegistry::parse().
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "config/NfcConfig.h"
#include "models/TagDefinition.h"
#include "models/TagIdentity.h"

namespace filament_station {
namespace models {

/// @brief Unprocessed data read directly from the PN532 for one tag, before
///        format parsing.
struct RawTagData {
  TagTechnology technology = TagTechnology::Unknown;  ///< Detected ISO14443A technology (NTAG21x, MIFARE Classic, ...).
  std::uint8_t uid[config::kNfcMaxUidLength]{};        ///< Raw UID bytes.
  std::uint8_t uidLength = 0;                          ///< Number of valid bytes in #uid.
  std::uint8_t sak = 0;                                ///< ISO14443A SAK byte from anticollision.
  bool ndefPresent = false;                            ///< Whether an NDEF message was found.
  bool ndefReadable = false;                            ///< Whether the NDEF message could be read.
  bool hardwareWritableKnown = false;                  ///< Whether physical write-capability could be determined.
  bool hardwareWritable = false;                       ///< Physical write-capability, only meaningful if #hardwareWritableKnown.
  std::uint16_t ndefLength = 0;                        ///< Number of valid bytes in #ndef.
  std::uint8_t ndef[config::kNfcMaxNdefBytes]{};        ///< Raw NDEF message bytes.
  // Decrypted MIFARE Classic blocks used by the documented Bambu format.
  // Bit N indicates whether block N was authenticated and read successfully.
  std::uint32_t mifareBlockMask = 0;      ///< Bit N set means #mifareBlocks[N] was authenticated and read.
  std::uint8_t mifareBlocks[17][16]{};    ///< Decrypted MIFARE Classic block contents, indexed by block number.
};

/// @brief What is safe to do with a specific tag, determined per read (see
///        docs/tag-identity.md).
struct TagCapabilities {
  bool canAssociateByUid = false;               ///< The identity may be used for a Spoolman extra.tag association.
  bool canWriteFilamentStationPayload = false;   ///< The native `spoolman:&lt;id&gt;` payload may be written.
  bool canClearFilamentStationPayload = false;   ///< The native payload may be erased.
  bool preserveOriginalContent = true;           ///< The physical tag content must not be modified (default for all foreign formats).
};

/// @brief Parsed result of one NFC read: format, identity, capabilities,
///        and the normalized filament definition (if any).
struct TagReadResult {
  TagTechnology technology = TagTechnology::Unknown;  ///< Detected ISO14443A technology.
  TagFormat format = TagFormat::Unknown;              ///< Detected payload format.
  std::uint8_t uid[config::kNfcMaxUidLength]{};        ///< Raw UID bytes.
  std::uint8_t uidLength = 0;                          ///< Number of valid bytes in #uid.
  // Captured when the tag is parsed and copied by value through the complete
  // workflow. It must not be derived again after an asynchronous operation.
  TagIdentity identity{};        ///< Canonical identity, frozen at parse time.
  bool ndefPresent = false;      ///< Whether an NDEF message was found.
  bool ndefReadable = false;     ///< Whether the NDEF message could be read.
  bool physicalWritableKnown = false;  ///< Whether physical write-capability could be determined.
  bool physicalWritable = false;       ///< Physical write-capability, only meaningful if #physicalWritableKnown.
  bool payloadValid = true;      ///< Whether the recognized format's payload parsed without error.
  bool writable = false;         ///< Whether the native FilamentStation payload may be written.
  bool erasable = false;         ///< Whether the native FilamentStation payload may be erased.
  bool knownFormat = false;      ///< Whether #format was identified (as opposed to Unknown).
  TagCapabilities capabilities{};  ///< Derived read/write capabilities for this tag.
  TagDefinition definition{};      ///< Normalized filament definition, if the format provides one.
};

static_assert(std::is_trivially_copyable<RawTagData>::value, "RawTagData must be trivially copyable");
static_assert(std::is_trivially_copyable<TagCapabilities>::value,
              "TagCapabilities must be trivially copyable");
static_assert(std::is_trivially_copyable<TagReadResult>::value, "TagReadResult must be trivially copyable");

}  // namespace models
}  // namespace filament_station
