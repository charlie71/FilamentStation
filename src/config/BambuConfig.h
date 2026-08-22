#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station::config {

// Local LAN-Mode MQTT interface exposed by Bambu Lab printers (X1/P1/A1
// series). Bambu Lab does not publish an official protocol specification;
// these values follow the widely used community reverse-engineering (e.g.
// OrcaSlicer, Home Assistant "Bambu Lab" integration, bambulabs_api). See
// docs/bambu-protocol.md for the full list of assumptions and their
// verification status against real hardware.
constexpr std::uint16_t kBambuMqttPort = 8883;
constexpr char kBambuMqttUsername[] = "bblp";
constexpr std::uint32_t kBambuConnectTimeoutMs = 8000;
// Bounded queue-wait between MQTT keepalive/service ticks; not a busy loop.
constexpr std::uint32_t kBambuServiceIntervalMs = 200;
constexpr std::uint32_t kBambuReconnectBackoffMs = 5000;
// How long BambuTask waits for the printer's own tray telemetry to confirm
// an AssignTray (ams_filament_setting + extrusion_cali_sel) before giving
// up and reporting failure -- publish() succeeding only means the MQTT
// broker accepted the packet, not that the printer applied it (e.g. a
// rejected/unsigned command, see docs/bambu-protocol.md). Printer reports
// observed roughly every 2-4s while idle, so this leaves comfortable
// margin for at least two report cycles.
constexpr std::uint32_t kBambuAssignConfirmTimeoutMs = 8000;
constexpr std::size_t kBambuTopicCapacity = 64;
constexpr std::size_t kBambuRequestPayloadCapacity = 256;

}  // namespace filament_station::config
