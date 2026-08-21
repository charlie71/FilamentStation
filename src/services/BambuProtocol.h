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

std::size_t bambuBuildPushAllRequest(char* output, std::size_t outputCapacity);

std::size_t bambuBuildAmsFilamentSetting(std::uint8_t amsId,
                                         std::uint8_t trayId,
                                         const BambuTrayFilament& filament,
                                         char* output,
                                         std::size_t outputCapacity);

// Merges recognized fields from a "report" topic payload into `state`.
// Returns false for payloads without a "print" object (not a status
// report, or unrecognized message type); such payloads are otherwise
// harmless and must not be treated as an error by the caller. `spoolId`
// entries in `state` are Spoolman associations unknown to the printer and
// are never modified here.
bool bambuApplyReport(const JsonDocument& document, models::PrinterState& state);

}  // namespace services
}  // namespace filament_station
