#pragma once

#include <cstdint>

namespace filament_station::rtos {

enum class UiCommandType : std::uint8_t { CommunicationTestResponse, ShowStatus };
enum class ScaleCommandType : std::uint8_t { Tare, StartCalibration, ResetCalibration, RequestMeasurement };
enum class NfcCommandType : std::uint8_t { StartReading, StopReading, WriteSpoolTag };
enum class StorageCommandType : std::uint8_t { LoadJson, SaveJson, DeleteJson, CreateBackup };
enum class NetworkCommandType : std::uint8_t { Connect, Reconfigure, StartPortal, ClearCredentials };
enum class SpoolmanCommandType : std::uint8_t { HealthCheck, LoadSpool, SearchSpools, UpdateWeight };
enum class BambuCommandType : std::uint8_t { Connect, Disconnect, RequestStatus, AssignTray };

}  // namespace filament_station::rtos

