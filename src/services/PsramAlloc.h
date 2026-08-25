#pragma once

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <new>

#include "services/Logger.h"

namespace filament_station::services {

// RAM optimization (2026-08-25, TASKS.md): several tasks keep large
// message-shaped structs (rtos::AppEvent and friends, each several KB) as
// function-local `static` buffers -- deliberately, to avoid stack-overflow
// crashes this project has hit before, see e.g. NfcTask.cpp's reportTag()
// comment. A `static` local always lands in internal DRAM (.bss is a
// link-time placement, independent of the runtime malloc-to-PSRAM
// threshold configured in this build, see docs/bambu-protocol.md-adjacent
// notes) -- with ~19 such buffers, several KB each, that is tens of KB of
// internal RAM permanently reserved for state that is only ever bulk-
// copied through a queue, never touched with fine-grained/timing-critical
// access. Moving them to PSRAM via heap_caps_malloc(MALLOC_CAP_SPIRAM)
// mirrors the pattern UiBridge.cpp::initializeLvgl() already uses for its
// (much larger) draw buffers.
//
// Allocates a single zero-initialized, PSRAM-backed instance of T, meant to
// replace a `static T x{};` declaration with `static T* x =
// allocatePsramInstance<T>("...");` (every subsequent `x.field` in that
// function becomes `x->field`). T must be default-constructible via
// aggregate/value init (`T{}`); every candidate this is used for is already
// static_assert'd trivially copyable elsewhere.
//
// PSRAM is not expected to ever actually run out for these modest,
// fixed-count allocations (a few dozen KB total against multiple MB of
// PSRAM) -- but a silently wrong pointer would be worse than a loud
// failure, so on allocation failure this halts the *calling* task (logs
// once, then blocks forever), the same fail-fast pattern already used for
// other unrecoverable init failures in this project (e.g. NfcTask's
// initializeUart(), ScaleTask's initializeHx711PinsAndInterrupt()) --
// never returns nullptr.
template <typename T>
T* allocatePsramInstance(const char* debugName) {
  void* raw =
      heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (raw == nullptr) {
    FS_LOGE(LogComponent::Rtos,
            "PSRAM allocation failed name=\"%s\" bytes=%u -- task halted",
            debugName, static_cast<unsigned>(sizeof(T)));
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }
  return new (raw) T{};
}

}  // namespace filament_station::services
