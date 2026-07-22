#pragma once

#include <cstdint>
#include <type_traits>
#include "rtos/Commands.h"
#include "rtos/Events.h"

namespace filament_station::rtos {

struct AppEvent {
  AppEventType type;
  std::uint32_t requestId;
  std::int32_t value;
  char text[64];
};

struct UiCommand {
  UiCommandType type;
  std::uint32_t requestId;
  float weightGrams;
  char title[48];
  char text[96];
};

struct ScaleCommand { ScaleCommandType type; std::uint32_t requestId; float referenceWeightGrams; };
struct NfcCommand { NfcCommandType type; std::uint32_t requestId; std::uint32_t spoolId; };

enum class StorageDocumentType : std::uint8_t { Device, Network, Spoolman, Bambu, Ui, Scale, Nfc, Diagnostics };
struct StorageCommand {
  StorageCommandType type;
  std::uint32_t requestId;
  char path[96];
  StorageDocumentType documentType;
};

struct NetworkCommand { NetworkCommandType type; std::uint32_t requestId; };
struct SpoolmanCommand { SpoolmanCommandType type; std::uint32_t requestId; std::uint32_t spoolId; float weightGrams; };
struct BambuCommand { BambuCommandType type; std::uint32_t requestId; std::uint8_t trayId; std::uint32_t spoolId; };

static_assert(std::is_trivially_copyable_v<AppEvent>);
static_assert(std::is_trivially_copyable_v<UiCommand>);

}  // namespace filament_station::rtos
