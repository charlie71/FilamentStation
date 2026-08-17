#pragma once

#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>

namespace filament_station::config {

struct TaskSettings {
  const char* name;
  std::uint32_t stackSize;
  UBaseType_t priority;
  BaseType_t core;
};

constexpr BaseType_t kNoCoreAffinity = tskNO_AFFINITY;

constexpr TaskSettings kStorageTask{"StorageTask", 8192, 2, kNoCoreAffinity};
constexpr TaskSettings kLoggingTask{"LoggingTask", 2048, 1, kNoCoreAffinity};
// AppEvent contains NFC parsing results and persisted UID mappings by value;
// UI workflows additionally create fixed-size UiCommand objects.
constexpr TaskSettings kAppTask{"AppTask", 8192, 3, kNoCoreAffinity};
constexpr TaskSettings kUiTask{"UiTask", 8192, 2, kNoCoreAffinity};
constexpr TaskSettings kScaleTask{"ScaleTask", 3072, 2, kNoCoreAffinity};
// PN532-Frames, wertbasierte TagReadResult/AppEvent-Nachrichten und die
// HKDF-SHA256-Schluesselableitung liegen zeitweise gleichzeitig im Stack.
constexpr TaskSettings kNfcTask{"NfcTask", 8192, 2, kNoCoreAffinity};
constexpr TaskSettings kNetworkTask{"NetworkTask", 4096, 1, kNoCoreAffinity};
constexpr TaskSettings kSpoolmanTask{"SpoolmanTask", 4096, 1, kNoCoreAffinity};
constexpr TaskSettings kBambuTask{"BambuTask", 4096, 1, kNoCoreAffinity};

// Queue-Laengen basieren auf geringer Last der Task-Gerueste und werden nach
// Messung der maximalen Auslastung in spaeteren Phasen angepasst.
constexpr UBaseType_t kAppEventQueueLength = 16;
constexpr UBaseType_t kUiCommandQueueLength = 8;
constexpr UBaseType_t kServiceCommandQueueLength = 8;
constexpr UBaseType_t kStorageCommandQueueLength = 8;
constexpr UBaseType_t kLogQueueLength = 32;
constexpr std::size_t kLogMessageCapacity = 256;

}  // namespace filament_station::config
