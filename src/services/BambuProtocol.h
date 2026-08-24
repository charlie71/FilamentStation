#pragma once

#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>

#include "models/PrinterState.h"

namespace filament_station {
namespace services {

// Pure encode/decode helpers for the Bambu LAN-Mode MQTT protocol. No
// network or storage access here (AGENTS.md coding rules); BambuTask owns
// the MQTT transport and calls into this file for topic names and JSON
// payload construction/interpretation. See docs/bambu-protocol.md for the
// (community-sourced, unverified) protocol assumptions this relies on.

struct BambuTrayFilament {
  char trayType[16]{};
  char trayColorHex[9]{};
  std::uint16_t nozzleTempMinC = 0;
  std::uint16_t nozzleTempMaxC = 0;
};

void bambuReportTopic(const char* serialNumber, char* output,
                      std::size_t outputCapacity);
void bambuRequestTopic(const char* serialNumber, char* output,
                       std::size_t outputCapacity);

// Normalizes a Spoolman-style 6-digit RRGGBB color to the wire's 8-digit
// RRGGBBAA form (alpha always FF per docs/bambu-protocol.md); an
// already-8-digit input passes through unchanged. Shared by
// bambuBuildAmsFilamentSetting() and BambuTask's pending-assignment
// confirmation check, so both compute the exact same expected value.
// `output` must have room for 9 bytes (8 hex digits + terminator).
void bambuNormalizeTrayColorHex(const char* input, char* output);

// sequenceId is sent as "print.sequence_id" (a string on the wire). The
// community reference (OpenBambuAPI) documents it as "incremented by 1 on
// each command"; this project used a hardcoded "0" for every request until
// a real hardware test showed the printer accepting-then-reverting a slot
// reassignment after a few seconds -- a repeated "0" is one still-unverified
// candidate explanation, see docs/bambu-protocol.md.
std::size_t bambuBuildPushAllRequest(std::uint32_t sequenceId, char* output,
                                     std::size_t outputCapacity);

std::size_t bambuBuildAmsFilamentSetting(std::uint32_t sequenceId,
                                         std::uint8_t amsId,
                                         std::uint8_t trayId,
                                         const BambuTrayFilament& filament,
                                         char* output,
                                         std::size_t outputCapacity);

// "extrusion_cali_sel" must follow a successful "ams_filament_setting" for a
// slot reassignment to actually persist -- confirmed via a comparable
// open-source ESP32 firmware (yanshay/spoolease) that reverse-engineers the
// same protocol; sending "ams_filament_setting" alone leaves the printer
// treating the change as provisional, which it silently discards after a
// few seconds. See docs/bambu-protocol.md. `trayInfoIdx` is echoed back as
// "filament_id"; `caliIdx` is -1 when no matching flow/pressure-advance
// calibration record is known (the common case for a plain material without
// a specific calibration profile). `nozzleDiameter` is the wire string
// (e.g. "0.4") read from the printer's own status report
// (`PrinterState::nozzleDiameter`); pass "0.4" if no report has arrived yet.
// `trayId` is the *local* slot index within `amsId` (0..kSlotsPerAms-1),
// same as bambuBuildAmsFilamentSetting()'s trayId -- this function encodes
// the wire's "tray_id" field as the *global* index (amsId * kSlotsPerAms +
// trayId) internally, since that command uses global addressing unlike
// ams_filament_setting (see the .cpp for the source of this distinction).
std::size_t bambuBuildExtrusionCaliSel(std::uint32_t sequenceId,
                                       std::uint8_t amsId,
                                       std::uint8_t trayId,
                                       const char* trayInfoIdx,
                                       const char* nozzleDiameter,
                                       std::int32_t caliIdx,
                                       char* output,
                                       std::size_t outputCapacity);

// Maps a free-text material name (e.g. Spoolman's filament.material field,
// "PLA"/"PETG"/"ABS"/"PLA-CF"/...) to Bambu's internal generic filament
// profile "setting_id" ("tray_info_idx" in ams_filament_setting). These are
// Bambu Studio's built-in *generic* (non-brand) profile ids -- community-
// documented via RFID-Tag-Guide/Home Assistant Bambu Lab integration, see
// docs/bambu-protocol.md. Returns an empty string for materials with no
// known generic mapping; never guesses a brand-specific id.
const char* bambuGenericTrayInfoIdx(const char* material);

// Merges recognized fields from a "report" topic payload into `state`.
// Returns false for payloads without a "print" object (not a status
// report, or unrecognized message type); such payloads are otherwise
// harmless and must not be treated as an error by the caller.
// `PrinterSlotStateData` deliberately has no `spoolId` field -- the printer
// has no notion of Spoolman identities (a project-specific attempt to
// round-trip one through a custom "tray_id_name" MQTT field was
// hardware-tested and abandoned, see docs/bambu-protocol.md). The
// printer/AMS/tray -> Spoolman-spool association is tracked and persisted
// entirely by AppTask instead, see models/TraySpoolCache.h.
bool bambuApplyReport(const JsonDocument& document, models::PrinterState& state);

}  // namespace services
}  // namespace filament_station
