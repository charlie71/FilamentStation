#pragma once

#include "models/TagReadResult.h"

namespace filament_station {
namespace nfc {

enum class TagAssignmentEffect {
  MappingOnly,
  MappingAndPayload,
};

inline bool isNativeNtag(models::TagTechnology technology) {
  return technology == models::TagTechnology::Ntag213 ||
         technology == models::TagTechnology::Ntag215 ||
         technology == models::TagTechnology::Ntag216;
}

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

inline void updateTagCapabilities(models::TagReadResult& tag) {
  tag.capabilities = capabilitiesFor(tag);
}

inline bool mayWriteTag(const models::TagReadResult& tag) {
  return tag.capabilities.canWriteFilamentStationPayload;
}

inline bool mayEraseTag(const models::TagReadResult& tag) {
  return tag.capabilities.canClearFilamentStationPayload;
}

inline TagAssignmentEffect assignmentEffect(
    const models::TagReadResult& tag) {
  return mayWriteTag(tag) ? TagAssignmentEffect::MappingAndPayload
                          : TagAssignmentEffect::MappingOnly;
}

inline TagAssignmentEffect removalEffect(const models::TagReadResult& tag) {
  return mayEraseTag(tag) ? TagAssignmentEffect::MappingAndPayload
                          : TagAssignmentEffect::MappingOnly;
}

}  // namespace nfc
}  // namespace filament_station
