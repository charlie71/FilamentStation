#pragma once

#include <cstdint>
#include <cstring>

#include "services/SpoolmanClient.h"

namespace filament_station {
namespace services {

enum class AssignmentDecision : std::uint8_t {
  Blocked,
  Assign,
  Idempotent,
  Reassign,
  Duplicate,
};

enum class RemovalDecision : std::uint8_t {
  Blocked,
  NotAssigned,
  Remove,
  Duplicate,
};

enum class NativeConsistency : std::uint8_t {
  Unavailable,
  MissingServerAssignment,
  Consistent,
  ConflictingSpool,
  Duplicate,
};

enum class LegacyMigrationDecision : std::uint8_t {
  SetTarget,
  AlreadyMigrated,
  Conflict,
};

inline bool tagOperationsAvailable(bool spoolmanReady, bool tagFieldReady) {
  return spoolmanReady && tagFieldReady;
}

inline AssignmentDecision assignmentDecision(bool available,
                                             TagLookupStatus lookup,
                                             std::uint32_t currentSpoolId,
                                             std::uint32_t targetSpoolId) {
  if (!available || targetSpoolId == 0) return AssignmentDecision::Blocked;
  if (lookup == TagLookupStatus::Duplicate)
    return AssignmentDecision::Duplicate;
  if (lookup == TagLookupStatus::NotFound) return AssignmentDecision::Assign;
  if (lookup != TagLookupStatus::Found || currentSpoolId == 0)
    return AssignmentDecision::Blocked;
  return currentSpoolId == targetSpoolId ? AssignmentDecision::Idempotent
                                         : AssignmentDecision::Reassign;
}

inline RemovalDecision removalDecision(bool available, TagLookupStatus lookup,
                                       std::uint32_t spoolId) {
  if (!available) return RemovalDecision::Blocked;
  if (lookup == TagLookupStatus::Duplicate) return RemovalDecision::Duplicate;
  if (lookup == TagLookupStatus::NotFound)
    return RemovalDecision::NotAssigned;
  return lookup == TagLookupStatus::Found && spoolId != 0
             ? RemovalDecision::Remove
             : RemovalDecision::Blocked;
}

inline NativeConsistency nativeConsistency(bool available,
                                           TagLookupStatus lookup,
                                           std::uint32_t payloadSpoolId,
                                           std::uint32_t serverSpoolId) {
  if (!available || payloadSpoolId == 0) return NativeConsistency::Unavailable;
  if (lookup == TagLookupStatus::Duplicate)
    return NativeConsistency::Duplicate;
  if (lookup == TagLookupStatus::NotFound)
    return NativeConsistency::MissingServerAssignment;
  if (lookup != TagLookupStatus::Found || serverSpoolId == 0)
    return NativeConsistency::Unavailable;
  return payloadSpoolId == serverSpoolId
             ? NativeConsistency::Consistent
             : NativeConsistency::ConflictingSpool;
}

inline LegacyMigrationDecision legacyMigrationDecision(
    bool targetFieldValid, const char* targetTag, const char* legacyTag) {
  if (!targetFieldValid || targetTag == nullptr || legacyTag == nullptr ||
      legacyTag[0] == '\0')
    return LegacyMigrationDecision::Conflict;
  if (targetTag[0] == '\0') return LegacyMigrationDecision::SetTarget;
  return std::strcmp(targetTag, legacyTag) == 0
             ? LegacyMigrationDecision::AlreadyMigrated
             : LegacyMigrationDecision::Conflict;
}

}  // namespace services
}  // namespace filament_station
