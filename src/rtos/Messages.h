#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "rtos/Commands.h"
#include "rtos/Events.h"

namespace filament_station::rtos {

struct AppEvent {
  AppEventType type;
  std::uint32_t requestId;
  std::int32_t value;
  std::int32_t scaleOffsetCounts;
  float scaleFactorCountsPerGram;
  bool scaleCalibrated;
  char text[64];
  UiAction uiAction;
};

struct UiCommand {
  UiCommandType type;
  UiOverlayKind overlayKind;
  std::uint32_t requestId;
  UiScreenId screenId;
  PrinterId printerId;
  SpoolId spoolId;
  std::uint8_t amsId;
  std::uint8_t trayId;
  std::int32_t value;
  float weightGrams;
  char title[48];
  char text[96];
};

struct ScaleCommand {
  ScaleCommandType type;
  std::uint32_t requestId;
  float referenceWeightGrams;
  std::int32_t offsetCounts;
  float factorCountsPerGram;
  bool calibrated;
};
struct NfcCommand { NfcCommandType type; std::uint32_t requestId; std::uint32_t spoolId; };

enum class StorageDocumentType : std::uint8_t { Device, Network, Spoolman, Bambu, Ui, Scale, Nfc, Diagnostics };
constexpr std::size_t kStorageJsonPayloadCapacity = 768;
struct StorageCommand {
  StorageCommandType type;
  std::uint32_t requestId;
  char path[96];
  StorageDocumentType documentType;
  std::uint16_t jsonLength;
  char json[kStorageJsonPayloadCapacity];
};

struct NetworkCommand { NetworkCommandType type; std::uint32_t requestId; };
struct SpoolmanCommand { SpoolmanCommandType type; std::uint32_t requestId; std::uint32_t spoolId; float weightGrams; };
struct BambuCommand {
  BambuCommandType type;
  std::uint32_t requestId;
  PrinterId printerId;
  std::uint8_t amsId;
  std::uint8_t trayId;
  SpoolId spoolId;
};

static_assert(std::is_trivially_copyable_v<AppEvent>);
static_assert(std::is_trivially_copyable_v<UiCommand>);
static_assert(std::is_trivially_copyable_v<UiAction>);
static_assert(std::is_trivially_copyable_v<StorageCommand>);

}  // namespace filament_station::rtos
