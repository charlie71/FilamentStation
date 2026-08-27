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

#include "models/PrinterState.h"

namespace filament_station {
namespace services {

/// @brief Filament fields written to a single AMS tray slot via
///        bambuBuildAmsFilamentSetting().
struct BambuTrayFilament {
  char trayType[16]{};             ///< Material name as sent on the wire (e.g. "PLA").
  char trayColorHex[9]{};          ///< 8-digit RRGGBBAA color, see bambuNormalizeTrayColorHex().
  std::uint16_t nozzleTempMinC = 0;  ///< Minimum recommended nozzle temperature.
  std::uint16_t nozzleTempMaxC = 0;  ///< Maximum recommended nozzle temperature.
};

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

/// @brief Builds an "ams_filament_setting" command payload to write one tray's filament data.
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
/// @param trayInfoIdx Generic filament profile id, echoed back as "filament_id" (see bambuGenericTrayInfoIdx()).
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

/// @brief Maps a free-text material name to Bambu's internal generic
///        filament profile "setting_id" ("tray_info_idx" in
///        ams_filament_setting).
/// @param material Free-text material name (e.g. Spoolman's
///        filament.material field: "PLA"/"PETG"/"ABS"/"PLA-CF"/...).
/// @return The mapped profile id, or an empty string if no known generic
///         mapping exists (never guesses a brand-specific id).
// These are Bambu Studio's built-in *generic* (non-brand) profile ids --
// community-documented via RFID-Tag-Guide/Home Assistant Bambu Lab
// integration, see docs/bambu-protocol.md.
const char* bambuGenericTrayInfoIdx(const char* material);

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
