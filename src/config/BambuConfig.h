/**
 * @file
 * @brief Bambu Lab LAN-Mode MQTT connection and protocol tuning constants
 *        (tasks::bambuTask(), services::BambuProtocol).
 */
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
constexpr std::uint16_t kBambuMqttPort = 8883;         ///< MQTTS port exposed by the printer in LAN mode.
constexpr char kBambuMqttUsername[] = "bblp";          ///< Fixed MQTT username; the LAN access code is the password.
constexpr std::uint32_t kBambuConnectTimeoutMs = 8000;  ///< Timeout for a single MQTT connect attempt.
// Bounded queue-wait between MQTT keepalive/service ticks; not a busy loop.
constexpr std::uint32_t kBambuServiceIntervalMs = 200;  ///< BambuTask's command-queue wait timeout, doubling as the MQTT service/keepalive tick.
// A dropped MQTT session (printer reboot, LAN hiccup) is retried on this
// interval by serviceConnections() for any printer still marked in-use
// (Robustheit/Diagnose, TASKS.md 10.4) -- was defined but never actually
// wired up before; only an explicit BambuCommandType::Connect (fired at
// boot and on WifiGotIp) ever reconnected. mqttClient.connect() itself
// blocks up to kBambuConnectTimeoutMs on failure, which already provides a
// natural floor on the retry cadence for a consistently slow-to-fail host.
constexpr std::uint32_t kBambuReconnectBackoffMs = 5000;  ///< Interval between automatic reconnect attempts for a printer that dropped its MQTT session.
// How long BambuTask waits for the printer's own tray telemetry to confirm
// an AssignTray (ams_filament_setting + extrusion_cali_sel) before giving
// up and reporting failure -- publish() succeeding only means the MQTT
// broker accepted the packet, not that the printer applied it (e.g. a
// rejected/unsigned command, see docs/bambu-protocol.md). Printer reports
// observed roughly every 2-4s while idle, so this leaves comfortable
// margin for at least two report cycles.
constexpr std::uint32_t kBambuAssignConfirmTimeoutMs = 8000;  ///< Timeout waiting for the printer's telemetry to confirm a pending AssignTray.
constexpr std::size_t kBambuTopicCapacity = 64;              ///< Buffer size for a formatted MQTT topic string.
constexpr std::size_t kBambuRequestPayloadCapacity = 256;    ///< Buffer size for a serialized outgoing MQTT JSON payload.

// K-factor upload (extrusion_cali_set/extrusion_cali_get/extrusion_cali_sel,
// see docs/bambu-protocol.md) -- hardware-unverified, community-derived from
// yanshay/spoolease. Same confirmation-timeout reasoning as
// kBambuAssignConfirmTimeoutMs, but tracked and timed out independently so a
// failed/slow K-factor upload never affects the (separately hardware-
// validated) AssignTray confirmation itself.
constexpr std::uint32_t kBambuCalibrationTimeoutMs = 8000;  ///< Timeout waiting for extrusion_cali_get to report the newly created calibration's cali_idx.
// extrusion_cali_set's payload has noticeably more fields than any other
// outgoing command (see BambuProtocol.cpp) -- a dedicated, larger capacity
// instead of enlarging the shared kBambuRequestPayloadCapacity for every
// other, smaller command.
constexpr std::size_t kBambuCalibrationRequestPayloadCapacity = 384;  ///< Buffer size for a serialized extrusion_cali_set payload.
constexpr std::size_t kBambuCalibrationSettingIdCapacity = 16;  ///< Buffer size for the self-assigned "setting_id" (e.g. "FS000004D2").
constexpr std::size_t kBambuCalibrationNameCapacity = 32;      ///< Buffer size for the cosmetic "name" field shown in Bambu Studio.

}  // namespace filament_station::config
