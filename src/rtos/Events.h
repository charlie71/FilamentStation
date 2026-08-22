#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace filament_station::rtos {

enum class AppEventType : std::uint8_t {
  UiAction,
  UiCommunicationTest,
  ScaleReady,
  ScaleMeasurement,
  ScaleStable,
  ScaleUnstable,
  ScaleTared,
  ScaleCalibrated,
  ScaleCalibrationReset,
  ScaleError,
  NfcInitialized,
  NfcTagDetected,
  NfcTagRemoved,
  NfcTagRead,
  NfcTagWritten,
  NfcTagErased,
  NfcError,
  SdMounted,
  SdRemoved,
  SdReinserted,
  SdError,
  StorageReadCompleted,
  StorageWriteCompleted,
  StorageRequestError,
  WifiStationConnected,
  WifiGotIp,
  WifiDisconnected,
  WifiLostIp,
  WifiConfigPortalStarted,
  WifiConfigPortalStopped,
  WifiConfigPortalTimedOut,
  WifiCredentialsCleared,
  SpoolmanConnected,
  SpoolmanTagFieldReady,
  SpoolmanTagLookup,
  SpoolmanTagDuplicate,
  SpoolmanTagUpdated,
  SpoolmanResponse,
  SpoolmanVendorResult,
  SpoolmanFilamentResult,
  SpoolmanCatalogCreated,
  SpoolmanCatalogDuplicate,
  SpoolmanImportCompleted,
  SpoolmanWeightUpdated,
  SpoolmanError,
  BambuConnected,
  BambuDisconnected,
  BambuUpdate,
  BambuTestResult,
  BambuError,
  // Periodic (throttled to ~1/s) remaining-time update while an AssignTray
  // awaits the printer's telemetry confirmation, see BambuTask::
  // serviceConnections(). "value" carries the remaining milliseconds.
  BambuAssignProgress
};

constexpr EventBits_t EVENT_UI_READY = BIT0;
constexpr EventBits_t EVENT_SD_READY = BIT1;
constexpr EventBits_t EVENT_SCALE_READY = BIT2;
constexpr EventBits_t EVENT_NFC_READY = BIT3;
constexpr EventBits_t EVENT_WIFI_CONNECTED = BIT4;
constexpr EventBits_t EVENT_SPOOLMAN_READY = BIT5;
constexpr EventBits_t EVENT_BAMBU_READY = BIT6;
constexpr EventBits_t EVENT_FATAL_ERROR = BIT7;
constexpr EventBits_t EVENT_SPOOLMAN_TAG_FIELD_READY = BIT8;

}  // namespace filament_station::rtos
