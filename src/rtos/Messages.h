#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "models/TagReadResult.h"
#include "rtos/Commands.h"
#include "rtos/Events.h"

namespace filament_station::rtos {

enum class NfcTagType : std::uint8_t {
  Unknown,
  Spoolman,
  Bambu,
  Legacy,
};

struct NfcUidMapping {
  std::uint8_t uid[10]{};
  std::uint8_t uidLength = 0;
  SpoolId spoolId = 0;
};
constexpr std::size_t kMaximumNfcUidMappings = 8;

struct AppEvent {
  AppEventType type;
  std::uint32_t requestId;
  std::int32_t value;
  std::int32_t scaleOffsetCounts;
  float scaleFactorCountsPerGram;
  bool scaleCalibrated;
  SpoolId spoolId;
  NfcTagType nfcTagType;
  std::uint8_t nfcUid[10];
  std::uint8_t nfcUidLength;
  models::TagReadResult tagReadResult;
  NfcUidMapping nfcMappings[kMaximumNfcUidMappings];
  std::uint8_t nfcMappingCount;
  // NFC diagnostics and multi-line UI status messages must remain complete.
  // Keep this fixed-size and value based; no pointers or Arduino Strings cross
  // task boundaries.
  char text[192];
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
  char text[192];
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
struct SpoolmanCommand {
  SpoolmanCommandType type;
  std::uint32_t requestId;
  std::uint32_t spoolId;
  float weightGrams;
  models::TagDefinition tagDefinition{};
};
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
