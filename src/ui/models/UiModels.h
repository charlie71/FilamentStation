#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "rtos/Commands.h"

namespace filament_station::ui::models {

constexpr std::size_t kPrinterNameCapacity = 32;
constexpr std::size_t kMaterialNameCapacity = 16;
constexpr std::size_t kVendorNameCapacity = 24;
constexpr std::size_t kFilamentNameCapacity = 32;
constexpr std::size_t kStatusTextCapacity = 32;
constexpr std::size_t kHostCapacity = 64;
constexpr std::size_t kSsidCapacity = 33;
constexpr std::size_t kIpAddressCapacity = 40;
constexpr std::size_t kMaximumAmsPerPrinter = 4;
constexpr std::size_t kTraysPerAms = 4;

enum class UiConnectionState : std::uint8_t {
  Disabled,
  Offline,
  Connecting,
  Connected,
  Error,
};

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

enum class UiStagingState : std::uint8_t {
  Empty,
  TagDetected,
  SpoolLoaded,
  WeightReady,
  Assigned,
  Error,
};

struct UiPrinterSummary {
  rtos::PrinterId printerId = 0;
  char name[kPrinterNameCapacity]{};
  UiConnectionState connectionState = UiConnectionState::Offline;
  std::uint8_t activeAmsId = 0;
  std::uint8_t amsCount = 0;
  bool isDefault = false;
  bool isSelected = false;
  bool enabled = true;
};

struct UiAmsSummary {
  rtos::PrinterId printerId = 0;
  std::uint8_t amsId = 0;
  std::uint8_t occupiedTrayCount = 0;
  std::uint8_t trayCount = kTraysPerAms;
  bool active = false;
  UiConnectionState connectionState = UiConnectionState::Offline;
};

struct UiTraySummary {
  rtos::PrinterId printerId = 0;
  std::uint8_t amsId = 0;
  std::uint8_t trayId = 0;
  rtos::SpoolId spoolId = 0;
  UiTrayState state = UiTrayState::Unknown;
  std::uint32_t colorRgb = 0;
  float remainingWeightGrams = 0.0F;
  char material[kMaterialNameCapacity]{};
  bool external = false;
  bool loaded = false;
  bool inUse = false;
};

struct UiStagingSummary {
  rtos::PrinterId printerId = 0;
  rtos::SpoolId spoolId = 0;
  UiStagingState state = UiStagingState::Empty;
  std::uint32_t colorRgb = 0;
  float grossWeightGrams = 0.0F;
  float remainingWeightGrams = 0.0F;
  char vendor[kVendorNameCapacity]{};
  char material[kMaterialNameCapacity]{};
  char nfcStatus[kStatusTextCapacity]{};
};

struct UiSpoolSummary {
  rtos::SpoolId spoolId = 0;
  char vendor[kVendorNameCapacity]{};
  char filament[kFilamentNameCapacity]{};
  char material[kMaterialNameCapacity]{};
  std::uint32_t colorRgb = 0;
  float emptyWeightGrams = 0.0F;
  float initialWeightGrams = 0.0F;
  float grossWeightGrams = 0.0F;
  float remainingWeightGrams = 0.0F;
  bool archived = false;
};

struct UiWeightState {
  float grossWeightGrams = 0.0F;
  float netWeightGrams = 0.0F;
  bool stable = false;
  bool calibrated = false;
  bool error = false;
  char status[kStatusTextCapacity]{};
};

struct UiSettingsState {
  char deviceName[kPrinterNameCapacity]{};
  char spoolmanHost[kHostCapacity]{};
  char wifiSsid[kSsidCapacity]{};
  char ipAddress[kIpAddressCapacity]{};
  rtos::PrinterId selectedPrinterId = 0;
  rtos::PrinterId defaultPrinterId = 0;
  UiConnectionState wifiState = UiConnectionState::Offline;
  UiConnectionState spoolmanState = UiConnectionState::Offline;
  bool scaleCalibrated = false;
};

static_assert(std::is_trivially_copyable_v<UiPrinterSummary>);
static_assert(std::is_trivially_copyable_v<UiAmsSummary>);
static_assert(std::is_trivially_copyable_v<UiTraySummary>);
static_assert(std::is_trivially_copyable_v<UiStagingSummary>);
static_assert(std::is_trivially_copyable_v<UiSpoolSummary>);
static_assert(std::is_trivially_copyable_v<UiWeightState>);
static_assert(std::is_trivially_copyable_v<UiSettingsState>);

}  // namespace filament_station::ui::models
