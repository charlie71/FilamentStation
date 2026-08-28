/**
 * @file
 * @brief Pure encode/decode helpers for the Bambu LAN-Mode MQTT protocol.
 *        No network or storage access here (AGENTS.md coding rules);
 *        BambuTask owns the MQTT transport and calls into this file for
 *        topic names and JSON payload construction/interpretation. See
 *        docs/bambu-protocol.md for the (community-sourced, unverified)
 *        protocol assumptions this relies on.
 */
#pragma once

#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>

#include "models/BambuMaterialMapping.h"
#include "models/PrinterState.h"

namespace filament_station {
namespace services {

/// @brief Filament fields written to a single AMS tray slot via
///        bambuBuildAmsFilamentSetting().
struct BambuTrayFilament {
  char trayInfoIdx[16]{};           ///< Bambu generic filament profile id ("tray_info_idx" on the wire), from BambuMaterialMappingEntry::trayInfoIdx.
  char trayType[16]{};              ///< Material name as sent on the wire (e.g. "PLA"), from BambuMaterialMappingEntry::trayType.
  char trayColorHex[9]{};           ///< 8-digit RRGGBBAA color, see bambuNormalizeTrayColorHex().
  std::uint16_t nozzleTempMinC = 0;  ///< Minimum nozzle temperature, from BambuMaterialMappingEntry::nozzleTempMinC.
  std::uint16_t nozzleTempMaxC = 0;  ///< Maximum nozzle temperature, from BambuMaterialMappingEntry::nozzleTempMaxC.
};

/// @brief Compares two material/alias names ignoring case and '-'/' '/'_'
///        separators (so "PLA-CF"/"PLA CF"/"PLACF" all compare equal).
/// @param a First name.
/// @param b Second name.
/// @return true if they normalize to the same key.
/// @note Exact (post-normalization) matching, never prefix matching -- a
///       prefix match would risk a more specific material (e.g. "PLA-CF")
///       being matched by a shorter, more general key ("PLA"). Exposed here
///       (rather than kept file-local) so services::BambuMaterialCatalog can
///       reuse the identical normalization when detecting duplicate
///       material/alias keys at parse time.
bool sameMaterialKey(const char* a, const char* b);

/// @brief Looks up the Bambu AMS profile for a free-text Spoolman material
///        name against a runtime-loaded mapping table.
/// @param table Mapping table loaded from /config/bambu_materials.json (see
///        services::BambuMaterialCatalog, tasks::storageTask()).
/// @param material Free-text material name (e.g. Spoolman's filament.material
///        field: "PLA"/"PETG"/"PLA-CF"/...). Matched via sameMaterialKey()
///        against each entry's canonical material name and its aliases, so
///        a more specific entry (e.g. "PLA-CF") is never confused with a
///        more general one (e.g. "PLA").
/// @return Pointer to the matching entry (valid as long as `table` is, i.e.
///         until the next atomic reload -- callers must not retain it past
///         the call that produced `table`), or nullptr if no explicit
///         mapping exists for this material -- never falls back to a
///         related/guessed material.
const models::BambuMaterialMappingEntry* resolveBambuMaterial(
    const models::BambuMaterialMappingTable& table, const char* material);

/// @brief Builds the MQTT topic the printer publishes status reports on.
/// @param serialNumber Printer serial number.
/// @param output Destination buffer receiving the topic string.
/// @param outputCapacity Size of `output` in bytes.
void bambuReportTopic(const char* serialNumber, char* output,
                      std::size_t outputCapacity);
/// @brief Builds the MQTT topic used to send requests/commands to the printer.
/// @param serialNumber Printer serial number.
/// @param output Destination buffer receiving the topic string.
/// @param outputCapacity Size of `output` in bytes.
void bambuRequestTopic(const char* serialNumber, char* output,
                       std::size_t outputCapacity);

/// @brief Normalizes a Spoolman-style 6-digit RRGGBB color to the wire's
///        8-digit RRGGBBAA form (alpha always FF per
///        docs/bambu-protocol.md); an already-8-digit input passes through
///        unchanged.
/// @param input 6- or 8-digit hex color, with or without a leading '#'.
/// @param output Destination buffer; must have room for 9 bytes (8 hex digits + terminator).
/// @note Shared by bambuBuildAmsFilamentSetting() and BambuTask's
///       pending-assignment confirmation check, so both compute the exact
///       same expected value.
void bambuNormalizeTrayColorHex(const char* input, char* output);

/// @brief Builds a "pushall" status-report request payload.
/// @param sequenceId Command sequence id, sent as "print.sequence_id" (a wire string).
/// @param output Destination buffer receiving the JSON payload.
/// @param outputCapacity Size of `output` in bytes.
/// @return Number of bytes written, or 0 on failure.
// sequenceId is sent as "print.sequence_id" (a string on the wire). The
// community reference (OpenBambuAPI) documents it as "incremented by 1 on
// each command"; this project used a hardcoded "0" for every request until
// a real hardware test showed the printer accepting-then-reverting a slot
// reassignment after a few seconds -- a repeated "0" is one still-unverified
// candidate explanation, see docs/bambu-protocol.md.
std::size_t bambuBuildPushAllRequest(std::uint32_t sequenceId, char* output,
                                     std::size_t outputCapacity);

/// @brief Builds an "ams_filament_setting" command payload to write one
///        tray's filament data. `filament.trayInfoIdx` is sent as-is (the
///        caller resolves it via resolveBambuMaterial()); no "setting_id"
///        field is ever sent -- that is a concrete Bambu-Studio-preset
///        concept this project deliberately does not resolve or send.
/// @param sequenceId Command sequence id, sent as "print.sequence_id".
/// @param amsId Target AMS unit index.
/// @param trayId Local slot index within `amsId` (0..kSlotsPerAms-1).
/// @param filament Filament fields to write.
/// @param output Destination buffer receiving the JSON payload.
/// @param outputCapacity Size of `output` in bytes.
/// @return Number of bytes written, or 0 on failure.
std::size_t bambuBuildAmsFilamentSetting(std::uint32_t sequenceId,
                                         std::uint8_t amsId,
                                         std::uint8_t trayId,
                                         const BambuTrayFilament& filament,
                                         char* output,
                                         std::size_t outputCapacity);

/// @brief Builds an "extrusion_cali_sel" command payload, required after
///        bambuBuildAmsFilamentSetting() for a slot reassignment to
///        actually persist.
/// @param sequenceId Command sequence id, sent as "print.sequence_id".
/// @param amsId Target AMS unit index.
/// @param trayId Local slot index within `amsId` (0..kSlotsPerAms-1); encoded on the wire as the *global* index (amsId * kSlotsPerAms + trayId), since this command uses global addressing unlike ams_filament_setting.
/// @param trayInfoIdx Generic filament profile id, echoed back as "filament_id" (see resolveBambuMaterial()).
/// @param nozzleDiameter Wire string (e.g. "0.4") read from the printer's own status report (`PrinterState::nozzleDiameter`); pass "0.4" if no report has arrived yet.
/// @param caliIdx Flow/pressure-advance calibration record id, or -1 if none is known (the common case for a plain material without a specific calibration profile).
/// @param output Destination buffer receiving the JSON payload.
/// @param outputCapacity Size of `output` in bytes.
/// @return Number of bytes written, or 0 on failure.
// "extrusion_cali_sel" must follow a successful "ams_filament_setting" for a
// slot reassignment to actually persist -- confirmed via a comparable
// open-source ESP32 firmware (yanshay/spoolease) that reverse-engineers the
// same protocol; sending "ams_filament_setting" alone leaves the printer
// treating the change as provisional, which it silently discards after a
// few seconds. See docs/bambu-protocol.md.
std::size_t bambuBuildExtrusionCaliSel(std::uint32_t sequenceId,
                                       std::uint8_t amsId,
                                       std::uint8_t trayId,
                                       const char* trayInfoIdx,
                                       const char* nozzleDiameter,
                                       std::int32_t caliIdx,
                                       char* output,
                                       std::size_t outputCapacity);

/// @brief Fields written when creating a new printer-side flow-dynamics
///        calibration profile via bambuBuildExtrusionCaliSet() -- the
///        precursor to a K-factor-carrying bambuBuildExtrusionCaliSel()
///        (see docs/bambu-protocol.md; hardware-unverified, derived from
///        yanshay/spoolease).
struct BambuCalibrationRequest {
  char filamentId[16]{};  ///< Generic filament profile id ("filament_id" on the wire), same value as BambuTrayFilament::trayInfoIdx.
  // Self-assigned, not printer-generated (yanshay/spoolease's "extrusion_cali_set"
  // example includes a caller-supplied "setting_id" on the create request
  // itself) -- deterministic per spool so a later bambuFindCalibrationBySettingId()
  // can match it back unambiguously. See BambuTask.cpp for how this is built.
  char settingId[16]{};  ///< Self-assigned unique id for this calibration profile (e.g. "FS000004D2"). Same capacity as config::kBambuCalibrationSettingIdCapacity (not referenced directly -- this header stays free of config/*.h, see BambuTrayFilament above for the same convention).
  char name[32]{};  ///< Cosmetic profile name, shown in Bambu Studio's calibration list. Same capacity as config::kBambuCalibrationNameCapacity.
  // Best-effort, unverified -- built from the printer's own reported
  // PrinterState::nozzleType + nozzleDiameter (e.g. "hardened_steel-0.4");
  // no verified type-to-code mapping (e.g. "HS00") is known, see
  // docs/bambu-protocol.md and PrinterState::nozzleType's doc comment.
  char nozzleId[24]{};    ///< Best-effort "nozzle_id" wire value.
  float kValue = 0.0F;    ///< Flow-dynamics K-factor to write ("k_value" on the wire, sent as a "%.6f" string).
};

/// @brief Builds an "extrusion_cali_set" command payload: creates a new
///        printer-side flow-dynamics calibration profile carrying a custom
///        K-factor. Does not assign it to any slot by itself -- the printer
///        assigns it a numeric "cali_idx" that must be looked up afterwards
///        (bambuBuildExtrusionCaliGet() + bambuFindCalibrationBySettingId())
///        and then applied via bambuBuildExtrusionCaliSel().
/// @param sequenceId Command sequence id, sent as "print.sequence_id".
/// @param amsId Target AMS unit index (only used for the external-slot address special-case, see bambuBuildExtrusionCaliSel()'s doc comment).
/// @param trayId Local slot index within `amsId`.
/// @param nozzleDiameter Wire string (e.g. "0.4"), same source as bambuBuildExtrusionCaliSel().
/// @param request Calibration fields to write.
/// @param output Destination buffer receiving the JSON payload.
/// @param outputCapacity Size of `output` in bytes (needs more room than
///        kBambuRequestPayloadCapacity, see config::kBambuCalibrationRequestPayloadCapacity).
/// @return Number of bytes written, or 0 on failure.
// Payload shape (print.filaments[0].{ams_id,extruder_id,filament_id,k_value,
// n_coef,name,nozzle_diameter,nozzle_id,setting_id,slot_id,tray_id},
// print.nozzle_diameter, print.sequence_id) taken from yanshay/spoolease's
// reverse-engineered "extrusion_cali_set" example -- hardware-unverified
// against this project's own printer, see docs/bambu-protocol.md.
// extruder_id is always 0 (single-extruder assumption, consistent with the
// rest of this project); n_coef is always "0.000000" (no corresponding
// Spoolman filament field exists); the inner filaments[0].tray_id is always
// -1 (the profile is not assigned to a slot by this command).
std::size_t bambuBuildExtrusionCaliSet(std::uint32_t sequenceId,
                                       std::uint8_t amsId,
                                       std::uint8_t trayId,
                                       const char* nozzleDiameter,
                                       const BambuCalibrationRequest& request,
                                       char* output,
                                       std::size_t outputCapacity);

/// @brief Builds an "extrusion_cali_get" command payload requesting the full
///        list of the printer's existing flow-dynamics calibration profiles
///        for one nozzle diameter (needed to look up the "cali_idx" a
///        preceding bambuBuildExtrusionCaliSet() was assigned, see
///        bambuFindCalibrationBySettingId()).
/// @param sequenceId Command sequence id, sent as "print.sequence_id".
/// @param nozzleDiameter Wire string (e.g. "0.4").
/// @param output Destination buffer receiving the JSON payload.
/// @param outputCapacity Size of `output` in bytes.
/// @return Number of bytes written, or 0 on failure.
// "print.filament_id" is always sent empty (unfiltered, full-list request) --
// matches yanshay/spoolease's own "extrusion_cali_get" request, which never
// populates it either.
std::size_t bambuBuildExtrusionCaliGet(std::uint32_t sequenceId,
                                       const char* nozzleDiameter,
                                       char* output,
                                       std::size_t outputCapacity);

/// @brief Result of bambuFindCalibrationBySettingId().
struct BambuCalibrationLookupResult {
  bool found = false;        ///< Whether a matching entry was found.
  std::int32_t caliIdx = 0;  ///< The matching entry's "cali_idx", only valid if #found.
};

/// @brief Searches a parsed "extrusion_cali_get" response for the entry
///        whose "setting_id" matches `settingId`, to learn the "cali_idx"
///        the printer assigned a just-created calibration profile (see
///        bambuBuildExtrusionCaliSet()).
/// @param document Parsed MQTT message payload (a report-topic message with
///        `print.command == "extrusion_cali_get"`).
/// @param settingId Self-assigned setting id to match (see
///        BambuCalibrationRequest::settingId) -- an exact string comparison,
///        safe because this id is never reused/collides with printer- or
///        Bambu-Studio-assigned ids (different, unrelated naming scheme).
/// @return The match, or `found == false` if the list is absent/empty or
///        contains no matching entry. Entries missing expected fields are
///        skipped rather than treated as an error.
// Iterates document["print"]["filaments"] -- the field name is carried over
// from yanshay/spoolease's own response handling, NOT verified against a
// real captured response from this project's printer (see
// docs/bambu-protocol.md); the most likely single point needing correction
// once this is tested against real hardware.
BambuCalibrationLookupResult bambuFindCalibrationBySettingId(
    const JsonDocument& document, const char* settingId);

/// @brief Merges recognized fields from a "report" topic payload into `state`.
/// @param document Parsed MQTT message payload.
/// @param state Printer state to update in place.
/// @return false for payloads without a "print" object (not a status
///         report, or unrecognized message type); such payloads are
///         otherwise harmless and must not be treated as an error by the caller.
// Also parses "print.ams.tray_now" into `state.activeTrayNow` (see
// models::kActiveTrayNowExternal/kActiveTrayNowNone) when present.
// `PrinterSlotStateData` deliberately has no `spoolId` field -- the printer
// has no notion of Spoolman identities (a project-specific attempt to
// round-trip one through a custom "tray_id_name" MQTT field was
// hardware-tested and abandoned, see docs/bambu-protocol.md). The
// printer/AMS/tray -> Spoolman-spool association is tracked and persisted
// entirely by AppTask instead, see models/TraySpoolCache.h.
bool bambuApplyReport(const JsonDocument& document, models::PrinterState& state);

}  // namespace services
}  // namespace filament_station
