/**
 * @file
 * @brief Central policy deciding which NFC-write/erase operations are safe
 *        for a given scanned tag, based on its format and physical
 *        writability. Version 1 only ever writes native NTAGs.
 */
#pragma once

#include "models/TagReadResult.h"

namespace filament_station {
namespace nfc {

/// @brief Whether assigning/removing a tag's spool mapping also rewrites
///        the tag's physical NDEF payload, or only updates the in-app
///        mapping (for formats FilamentStation must not touch).
enum class TagAssignmentEffect {
  MappingOnly,        ///< Only the app-side UID-to-spool mapping changes; the tag itself is left untouched.
  MappingAndPayload,  ///< The tag's physical NDEF payload is rewritten/erased in addition to the mapping.
};

/// @brief Whether a tag technology is one of the native NTAGs FilamentStation
///        is allowed to write to.
/// @param technology Detected tag technology.
/// @return true for NTAG213/215/216.
inline bool isNativeNtag(models::TagTechnology technology) {
  return technology == models::TagTechnology::Ntag213 ||
         technology == models::TagTechnology::Ntag215 ||
         technology == models::TagTechnology::Ntag216;
}

/// @brief Derives the write/erase/preserve capabilities for a scanned tag
///        from its format and physical writability.
/// @param tag Fully parsed tag read result.
/// @return The capability set to store in TagReadResult::capabilities.
inline models::TagCapabilities capabilitiesFor(
    const models::TagReadResult& tag) {
  models::TagCapabilities capabilities{};
  capabilities.canAssociateByUid = tag.uidLength > 0;

  const bool safelyWritableNativeTag =
      isNativeNtag(tag.technology) && tag.payloadValid &&
      tag.physicalWritableKnown && tag.physicalWritable;

  switch (tag.format) {
    case models::TagFormat::EmptyNdef:
      capabilities.canWriteFilamentStationPayload = safelyWritableNativeTag;
      capabilities.preserveOriginalContent = false;
      break;
    case models::TagFormat::FilamentStation:
      capabilities.canWriteFilamentStationPayload = safelyWritableNativeTag;
      capabilities.canClearFilamentStationPayload = safelyWritableNativeTag;
      capabilities.preserveOriginalContent = false;
      break;
    case models::TagFormat::Legacy:
      capabilities.canWriteFilamentStationPayload =
          safelyWritableNativeTag &&
          tag.definition.safeToRewriteAsFilamentStation;
      capabilities.preserveOriginalContent =
          !capabilities.canWriteFilamentStationPayload;
      break;
    case models::TagFormat::BambuLab:
    case models::TagFormat::OpenPrintTag:
    case models::TagFormat::OpenTag3D:
    case models::TagFormat::Unknown:
      // Version 1 associates these formats only by UID. Their original data
      // is never modified, even when the hardware reports a writable tag.
      break;
  }
  return capabilities;
}

/// @brief Recomputes and stores a tag's write/erase capabilities in place.
/// @param tag Tag read result to update; its TagReadResult::capabilities field is overwritten.
inline void updateTagCapabilities(models::TagReadResult& tag) {
  tag.capabilities = capabilitiesFor(tag);
}

/// @brief Whether the FilamentStation payload may be written to this tag.
/// @param tag Fully parsed tag read result.
/// @return true if capabilitiesFor() allows a payload write.
inline bool mayWriteTag(const models::TagReadResult& tag) {
  return tag.capabilities.canWriteFilamentStationPayload;
}

/// @brief Whether the FilamentStation payload may be erased from this tag.
/// @param tag Fully parsed tag read result.
/// @return true if capabilitiesFor() allows a payload erase.
inline bool mayEraseTag(const models::TagReadResult& tag) {
  return tag.capabilities.canClearFilamentStationPayload;
}

/// @brief What assigning a spool to this tag will actually do.
/// @param tag Fully parsed tag read result.
/// @return MappingAndPayload if the tag is writable, otherwise MappingOnly.
inline TagAssignmentEffect assignmentEffect(
    const models::TagReadResult& tag) {
  return mayWriteTag(tag) ? TagAssignmentEffect::MappingAndPayload
                          : TagAssignmentEffect::MappingOnly;
}

/// @brief What removing this tag's spool mapping will actually do.
/// @param tag Fully parsed tag read result.
/// @return MappingAndPayload if the tag is erasable, otherwise MappingOnly.
inline TagAssignmentEffect removalEffect(const models::TagReadResult& tag) {
  return mayEraseTag(tag) ? TagAssignmentEffect::MappingAndPayload
                          : TagAssignmentEffect::MappingOnly;
}

}  // namespace nfc
}  // namespace filament_station
