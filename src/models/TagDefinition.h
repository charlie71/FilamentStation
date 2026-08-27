/**
 * @file
 * @brief NFC tag technology/format classification and the normalized
 *        filament definition parsed from a tag's payload.
 */
#pragma once

#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

/// @brief Physical NFC technology detected via ISO14443A anticollision.
enum class TagTechnology : std::uint8_t {
  Unknown,          ///< Not yet determined or unrecognized.
  Ntag213,          ///< NXP NTAG213 (144-byte NDEF memory).
  Ntag215,          ///< NXP NTAG215 (496-byte NDEF memory).
  Ntag216,          ///< NXP NTAG216 (872-byte NDEF memory).
  MifareClassic1K,  ///< MIFARE Classic 1K (used by original Bambu Lab tags).
  MifareClassic4K,  ///< MIFARE Classic 4K.
  OtherIso14443A,   ///< Any other ISO14443A tag not specifically recognized.
};

/// @brief Payload format identified from a tag's NDEF/MIFARE content.
enum class TagFormat : std::uint8_t {
  Unknown,          ///< Format could not be identified.
  EmptyNdef,        ///< Physically writable tag with no NDEF message yet.
  FilamentStation,  ///< Native `spoolman:&lt;id&gt;` payload.
  BambuLab,         ///< Original Bambu Lab RFID (read-only, see docs/bambu-rfid.md).
  OpenPrintTag,      ///< OpenPrintTag CBOR/MIME payload (read-only, see docs/openprinttag.md).
  OpenTag3D,         ///< OpenTag3D binary memory-map payload (read-only, see docs/opentag3d.md).
  Legacy,            ///< Older `spool:&lt;id&gt;` payload (see docs/legacy-and-unknown-tags.md).
};

/// @brief Filament data normalized from any recognized tag format into a
///        common shape, for display and Spoolman import.
struct TagDefinition {
  TagFormat format = TagFormat::Unknown;  ///< Format this definition was parsed from.
  bool hasSpoolId = false;                ///< Whether #spoolId is a valid native FilamentStation reference.
  // Set only by a concrete, verified legacy parser that explicitly permits
  // replacing its payload with the native FilamentStation reference.
  bool safeToRewriteAsFilamentStation = false;  ///< Whether this tag's payload may be migrated to the native format.
  std::uint32_t spoolId = 0;      ///< Referenced Spoolman spool id, only valid if #hasSpoolId.
  char vendor[48]{};              ///< Filament vendor/manufacturer name, if known.
  char filamentName[48]{};        ///< Detailed filament product name, if known.
  char material[32]{};            ///< Material family (e.g. "PLA", "Support for PLA").
  char colorName[48]{};           ///< Human-readable color name, if the format provides one.
  char colorCode[12]{};           ///< Color as "#RRGGBB", if known.
  float nominalFilamentWeightG = 0.0F;  ///< Nominal net filament weight in grams.
  float emptySpoolWeightG = 0.0F;       ///< Empty spool weight in grams, if the format provides it.
  std::int16_t nozzleTempMinC = 0;      ///< Minimum recommended nozzle temperature, if known.
  std::int16_t nozzleTempMaxC = 0;      ///< Maximum recommended nozzle temperature, if known.
  char sourceDescription[64]{};         ///< Short human-readable description of where this definition came from.
};

static_assert(std::is_trivially_copyable<TagDefinition>::value, "TagDefinition must be trivially copyable");

}  // namespace models
}  // namespace filament_station
