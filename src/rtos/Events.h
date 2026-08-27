/**
 * @file
 * @brief The single message type carried on rtos::RtosContext::appEventQueue,
 *        and the FreeRTOS event-group readiness/error bits shared across
 *        all tasks. See docs/architecture.md, section "Events".
 */
#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace filament_station::rtos {

/// @brief Every event a service task can report to tasks::appTask() via
///        rtos::AppEvent. Grouped by source; see docs/architecture.md for
///        the full grouping table. Most variants are self-explanatory from
///        their name; only variants with a non-obvious `value`/`text`
///        payload carry their own comment below.
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
  BambuAssignProgress,
  // Firmware-Update-Versions-Check (TASKS.md Phase 13.2): "value" ist 1
  // (Update verfuegbar), 0 (Firmware aktuell) oder -1 (Fehler); "text"
  // traegt bei 1 die verfuegbare Version, bei -1 die Fehlermeldung.
  UpdateCheckResult,
  // Firmware-Update-Download (TASKS.md Phase 13.3): "value" ist der
  // Fortschritt in Prozent (0-100), throttled auf
  // kUpdateProgressReportIntervalMs.
  UpdateDownloadProgress,
  // "value" ist 1 (erfolgreich, Neustart folgt erst in Phase 13.5) oder 0
  // (Fehler, "text" traegt die Fehlermeldung).
  UpdateDownloadResult
};

constexpr EventBits_t EVENT_UI_READY = BIT0;                     ///< UiTask has completed LVGL/display initialization.
constexpr EventBits_t EVENT_SD_READY = BIT1;                     ///< SD card is mounted and its directory structure is valid.
constexpr EventBits_t EVENT_SCALE_READY = BIT2;                  ///< HX711 is producing valid samples.
constexpr EventBits_t EVENT_NFC_READY = BIT3;                    ///< PN532 initialized successfully.
constexpr EventBits_t EVENT_WIFI_CONNECTED = BIT4;                ///< WiFi station is connected with an IP address.
constexpr EventBits_t EVENT_SPOOLMAN_READY = BIT5;                ///< Spoolman server responded to a health check.
constexpr EventBits_t EVENT_BAMBU_READY = BIT6;                   ///< Reserved; not currently set by any task.
constexpr EventBits_t EVENT_FATAL_ERROR = BIT7;                  ///< An unrecoverable startup error occurred; halts the affected task.
constexpr EventBits_t EVENT_SPOOLMAN_TAG_FIELD_READY = BIT8;      ///< Spoolman's `extra.tag` field exists and is usable.

}  // namespace filament_station::rtos
