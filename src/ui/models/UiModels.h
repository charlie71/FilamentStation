/**
 * @file
 * @brief LVGL-facing display value types used by UiBridge.cpp: compact,
 *        trivially-copyable snapshots of printer/tray/staging/spool state
 *        formatted for direct rendering, distinct from the rtos:: wire
 *        types they are derived from.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "rtos/Commands.h"

namespace filament_station::ui::models {

constexpr std::size_t kPrinterNameCapacity = 32;   ///< Buffer size for a printer display name.
constexpr std::size_t kMaterialNameCapacity = 16;  ///< Buffer size for a material name.
constexpr std::size_t kVendorNameCapacity = 24;    ///< Buffer size for a vendor name.
constexpr std::size_t kFilamentNameCapacity = 32;  ///< Buffer size for a filament name.
constexpr std::size_t kStatusTextCapacity = 32;    ///< Buffer size for a short status text.
constexpr std::size_t kHostCapacity = 64;          ///< Buffer size for a host/IP string.
constexpr std::size_t kSsidCapacity = 33;          ///< Buffer size for a WiFi SSID.
constexpr std::size_t kIpAddressCapacity = 40;     ///< Buffer size for an IP address string.
constexpr std::size_t kMaximumAmsPerPrinter = 4;   ///< Maximum AMS units rendered per printer.
constexpr std::size_t kTraysPerAms = 4;            ///< Trays rendered per AMS unit.
constexpr std::size_t kMaximumFilamentColors = 3;  ///< Maximum colors rendered for a multi-color filament.

/// @brief Display-facing connection/readiness state (printer, WiFi, Spoolman).
enum class UiConnectionState : std::uint8_t {
  Disabled,
  Offline,
  Connecting,
  Connected,
  Error,
};

/// @brief Display-facing occupancy/activity state of one AMS tray.
enum class UiTrayState : std::uint8_t {
  Unknown,
  Empty,
  Reading,
  Ready,
  Loading,
  Loaded,
  Unloading,
  Error,
};

/// @brief Display-facing state of the currently staged spool.
enum class UiStagingState : std::uint8_t {
  Empty,
  TagDetected,
  SpoolLoaded,
  WeightReady,
  Assigned,
  Error,
};

/// @brief Display snapshot of one configured printer, for the printer list/header.
struct UiPrinterSummary {
  rtos::PrinterId printerId = 0;   ///< Printer this summary describes.
  char name[kPrinterNameCapacity]{};  ///< Display name.
  UiConnectionState connectionState = UiConnectionState::Offline;  ///< Current connection state.
  std::uint8_t activeAmsId = 0;    ///< AMS unit currently shown as active.
  std::uint8_t amsCount = 0;       ///< Number of present AMS units.
  bool isDefault = false;          ///< Whether this is the default printer.
  bool isSelected = false;         ///< Whether this printer is currently selected/shown.
  bool enabled = true;             ///< Whether this printer is enabled.
};

/// @brief Display snapshot of one AMS unit, for the AMS overview.
struct UiAmsSummary {
  rtos::PrinterId printerId = 0;   ///< Owning printer.
  std::uint8_t amsId = 0;          ///< AMS unit index.
  std::uint8_t occupiedTrayCount = 0;  ///< Number of trays currently occupied.
  std::uint8_t trayCount = kTraysPerAms;  ///< Total trays in this unit.
  bool active = false;             ///< Whether this unit is currently selected/shown.
  UiConnectionState connectionState = UiConnectionState::Offline;  ///< Current connection state.
};

/// @brief Display snapshot of one tray, for the tray detail card.
struct UiTraySummary {
  rtos::PrinterId printerId = 0;   ///< Owning printer.
  std::uint8_t amsId = 0;          ///< Owning AMS unit index.
  std::uint8_t trayId = 0;         ///< Tray index within its AMS unit.
  rtos::SpoolId spoolId = 0;       ///< Resolved Spoolman spool id, or 0 if unknown.
  UiTrayState state = UiTrayState::Unknown;  ///< Current occupancy/activity state.
  std::array<std::uint32_t, kMaximumFilamentColors> colorRgb{};  ///< Rendered color(s), RGB888.
  std::uint8_t colorCount = 0;     ///< Number of valid entries in #colorRgb.
  float remainingWeightGrams = 0.0F;  ///< Remaining filament weight, if known.
  char material[kMaterialNameCapacity]{};  ///< Material name reported by the printer.
  bool external = false;           ///< Whether this represents the external/manual spool holder.
  bool loaded = false;             ///< Whether the tray currently holds filament.
  bool inUse = false;              ///< Whether this tray is currently loaded into the nozzle.
};

/// @brief Display snapshot of the currently staged spool, for the staging card.
struct UiStagingSummary {
  rtos::PrinterId printerId = 0;   ///< Printer the staged spool is intended for, if any.
  rtos::SpoolId spoolId = 0;       ///< Staged spool id.
  UiStagingState state = UiStagingState::Empty;  ///< Current staging state.
  std::array<std::uint32_t, kMaximumFilamentColors> colorRgb{};  ///< Rendered color(s), RGB888.
  std::uint8_t colorCount = 0;     ///< Number of valid entries in #colorRgb.
  float grossWeightGrams = 0.0F;   ///< Measured gross weight, if any.
  float remainingWeightGrams = 0.0F;  ///< Computed/known remaining weight.
  // K-Faktor ist eine Spoolman *Filament*-Eigenschaft (Nutzerhinweis
  // 2026-08-24), asynchron nachgeladen -- kFactorValid unterscheidet "noch
  // nicht/nicht verfuegbar" von einem tatsaechlichen K-Faktor 0.
  bool kFactorValid = false;       ///< Whether #kFactor has been loaded yet.
  float kFactor = 0.0F;            ///< Loaded flow-dynamics K-factor, only valid if #kFactorValid.
  char vendor[kVendorNameCapacity]{};    ///< Vendor name.
  char material[kMaterialNameCapacity]{};  ///< Material name.
  char nfcStatus[kStatusTextCapacity]{};   ///< NFC tag status text for the staged spool.
};

/// @brief Display snapshot of one spool, for the spool picker/detail screens.
struct UiSpoolSummary {
  rtos::SpoolId spoolId = 0;       ///< Spool this summary describes.
  char vendor[kVendorNameCapacity]{};      ///< Vendor name.
  char filament[kFilamentNameCapacity]{};  ///< Filament name.
  char material[kMaterialNameCapacity]{};  ///< Material name.
  std::array<std::uint32_t, kMaximumFilamentColors> colorRgb{};  ///< Rendered color(s), RGB888.
  std::uint8_t colorCount = 0;     ///< Number of valid entries in #colorRgb.
  float emptyWeightGrams = 0.0F;   ///< Empty-spool weight.
  float initialWeightGrams = 0.0F;  ///< Initial filament weight.
  float grossWeightGrams = 0.0F;   ///< Gross (spool + filament) weight.
  float remainingWeightGrams = 0.0F;  ///< Remaining filament weight.
  bool archived = false;           ///< Whether the spool is archived in Spoolman.
};

/// @brief Display snapshot of the current scale reading, for the weight card.
struct UiWeightState {
  float grossWeightGrams = 0.0F;   ///< Raw measured weight.
  float netWeightGrams = 0.0F;     ///< Weight with tare applied.
  bool stable = false;             ///< Whether the reading is currently stable.
  bool calibrated = false;         ///< Whether the scale is calibrated.
  bool error = false;              ///< Whether the scale is reporting an error.
  char status[kStatusTextCapacity]{};  ///< Status text.
};

/// @brief Display snapshot of device/network/Spoolman settings, for the settings screens.
struct UiSettingsState {
  char deviceName[kPrinterNameCapacity]{};  ///< Device display name.
  char spoolmanHost[kHostCapacity]{};       ///< Configured Spoolman host/URL.
  char wifiSsid[kSsidCapacity]{};           ///< Connected/configured WiFi SSID.
  char ipAddress[kIpAddressCapacity]{};     ///< Current device IP address.
  rtos::PrinterId selectedPrinterId = 0;    ///< Currently selected printer.
  rtos::PrinterId defaultPrinterId = 0;     ///< Configured default printer.
  UiConnectionState wifiState = UiConnectionState::Offline;      ///< Current WiFi connection state.
  UiConnectionState spoolmanState = UiConnectionState::Offline;  ///< Current Spoolman connection state.
  bool scaleCalibrated = false;             ///< Whether the scale is calibrated.
};

static_assert(std::is_trivially_copyable_v<UiPrinterSummary>);
static_assert(std::is_trivially_copyable_v<UiAmsSummary>);
static_assert(std::is_trivially_copyable_v<UiTraySummary>);
static_assert(std::is_trivially_copyable_v<UiStagingSummary>);
static_assert(std::is_trivially_copyable_v<UiSpoolSummary>);
static_assert(std::is_trivially_copyable_v<UiWeightState>);
static_assert(std::is_trivially_copyable_v<UiSettingsState>);

}  // namespace filament_station::ui::models
