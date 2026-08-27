/**
 * @file
 * @brief Pure decision functions for tag-to-spool assignment/removal and
 *        legacy-mapping migration. Every function here is a stateless
 *        lookup-table over its inputs, not a held state machine.
 */
#pragma once

#include <cstdint>
#include <cstring>

#include "services/SpoolmanClient.h"

namespace filament_station {
namespace services {

/// @brief Outcome of assignmentDecision().
enum class AssignmentDecision : std::uint8_t {
  Blocked,     ///< Assignment is not currently possible (Spoolman/tag-field unavailable, or no target spool).
  Assign,      ///< Tag has no existing assignment; a new one can be created.
  Idempotent,  ///< Tag is already assigned to the target spool; no change needed.
  Reassign,    ///< Tag is assigned to a different spool; assignment would replace it.
  Duplicate,   ///< Server reports more than one assignment for this tag identity.
};

/// @brief Outcome of removalDecision().
enum class RemovalDecision : std::uint8_t {
  Blocked,      ///< Removal is not currently possible (Spoolman/tag-field unavailable).
  NotAssigned,  ///< Tag has no assignment to remove.
  Remove,       ///< Tag has a valid assignment that can be removed.
  Duplicate,    ///< Server reports more than one assignment for this tag identity.
};

/// @brief Outcome of nativeConsistency(): whether a tag's own payload
///        agrees with the server-side assignment for its identity.
enum class NativeConsistency : std::uint8_t {
  Unavailable,              ///< Consistency cannot be determined (Spoolman unavailable, or no payload spool id).
  MissingServerAssignment,  ///< The tag's payload claims a spool, but the server has no assignment for this identity.
  Consistent,               ///< The tag's payload spool id matches the server assignment.
  ConflictingSpool,         ///< The tag's payload spool id differs from the server assignment.
  Duplicate,                ///< Server reports more than one assignment for this tag identity.
};

/// @brief Outcome of legacyMigrationDecision(): whether a target identity
///        field can be set from a discovered legacy tag value.
enum class LegacyMigrationDecision : std::uint8_t {
  SetTarget,       ///< Target field is empty; the legacy value can be written into it.
  AlreadyMigrated, ///< Target field already holds the same value as the legacy tag.
  Conflict,        ///< Target field holds a different value, or inputs are invalid.
};

/// @brief Whether tag-assignment operations are currently possible at all.
/// @param spoolmanReady Whether the Spoolman server is reachable.
/// @param tagFieldReady Whether Spoolman's `extra.tag` custom field exists.
/// @return true if both preconditions are met.
inline bool tagOperationsAvailable(bool spoolmanReady, bool tagFieldReady) {
  return spoolmanReady && tagFieldReady;
}

/// @brief Decides what assigning a tag to a target spool would do.
/// @param available Result of tagOperationsAvailable().
/// @param lookup Server-side lookup status for the tag's identity.
/// @param currentSpoolId Spool id the tag is currently assigned to on the server (0 if none).
/// @param targetSpoolId Spool id the caller wants to assign.
/// @return The resulting decision.
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

/// @brief Decides what removing a tag's assignment would do.
/// @param available Result of tagOperationsAvailable().
/// @param lookup Server-side lookup status for the tag's identity.
/// @param spoolId Spool id the tag is currently assigned to on the server (0 if none).
/// @return The resulting decision.
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

/// @brief Checks whether a tag's own on-tag payload agrees with the
///        server-side assignment for its identity.
/// @param available Result of tagOperationsAvailable().
/// @param lookup Server-side lookup status for the tag's identity.
/// @param payloadSpoolId Spool id encoded on the physical tag (0 if none).
/// @param serverSpoolId Spool id the server has assigned to this identity (0 if none).
/// @return The resulting consistency classification.
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

/// @brief Decides whether a legacy NFC-mapping tag value can be migrated
///        into a target identity field.
/// @param targetFieldValid Whether the target field/buffer itself is usable.
/// @param targetTag Current value of the target field (NUL-terminated).
/// @param legacyTag Legacy tag value discovered during migration (NUL-terminated).
/// @return The resulting migration decision.
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
