/**
 * @file
 * @brief The FreeRTOS object registry (queues, event group, task handles)
 *        shared by every task, plus the process-wide logging transport.
 *        See docs/architecture.md, section "Tasks & Queues".
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include "config/TaskConfig.h"
#include "models/BambuMaterialMapping.h"

namespace filament_station::rtos {

/// @brief One line of text passed through the log queue to services::Logger.
struct LogMessage {
  char text[config::kLogMessageCapacity]{};  ///< NUL-terminated, already-formatted log line.
};

/// @brief All FreeRTOS queues, the shared event group, and every task
///        handle. A single global instance is created by rtos::context()
///        and populated by #createObjects()/#createTasks() during startup.
struct RtosContext {
  QueueHandle_t appEventQueue = nullptr;      ///< Service tasks -> AppTask, carries rtos::AppEvent.
  QueueHandle_t uiCommandQueue = nullptr;     ///< AppTask -> UiTask, carries rtos::UiCommand.
  QueueHandle_t scaleCommandQueue = nullptr;  ///< AppTask -> ScaleTask, carries rtos::ScaleCommand.
  QueueHandle_t nfcCommandQueue = nullptr;    ///< AppTask -> NfcTask, carries rtos::NfcCommand.
  QueueHandle_t storageCommandQueue = nullptr;  ///< AppTask -> StorageTask, carries rtos::StorageCommand.
  QueueHandle_t networkCommandQueue = nullptr;  ///< AppTask -> NetworkTask, carries rtos::NetworkCommand.
  QueueHandle_t wifiEventQueue = nullptr;       ///< WiFi driver callback -> NetworkTask, carries a raw status byte.
  QueueSetHandle_t networkQueueSet = nullptr;   ///< Set combining #networkCommandQueue and #wifiEventQueue so NetworkTask can block on either.
  QueueHandle_t spoolmanCommandQueue = nullptr;  ///< AppTask -> SpoolmanTask, carries rtos::SpoolmanCommand.
  QueueHandle_t bambuCommandQueue = nullptr;     ///< AppTask -> BambuTask, carries rtos::BambuCommand.
  QueueHandle_t powerCommandQueue = nullptr;     ///< AppTask/peripheral tasks <-> PowerTask, carries rtos::PowerCommand.
  QueueHandle_t updateCommandQueue = nullptr;    ///< AppTask -> UpdateTask, carries rtos::UpdateCommand.
  QueueHandle_t logQueue = nullptr;              ///< Every task -> LoggingTask, carries #LogMessage.
  EventGroupHandle_t systemEventGroup = nullptr;  ///< Shared readiness/error bits, see rtos::EVENT_*.
  TaskHandle_t loggingTask = nullptr;   ///< Handle for services::Logger::task.
  TaskHandle_t uiTask = nullptr;        ///< Handle for tasks::uiTask.
  TaskHandle_t appTask = nullptr;       ///< Handle for tasks::appTask.
  TaskHandle_t scaleTask = nullptr;     ///< Handle for tasks::scaleTask.
  TaskHandle_t nfcTask = nullptr;       ///< Handle for tasks::nfcTask.
  TaskHandle_t storageTask = nullptr;   ///< Handle for tasks::storageTask.
  TaskHandle_t networkTask = nullptr;   ///< Handle for tasks::networkTask.
  TaskHandle_t spoolmanTask = nullptr;  ///< Handle for tasks::spoolmanTask.
  TaskHandle_t bambuTask = nullptr;     ///< Handle for tasks::bambuTask.
  TaskHandle_t powerTask = nullptr;     ///< Handle for tasks::powerTask.
  TaskHandle_t updateTask = nullptr;    ///< Handle for tasks::updateTask.

  // Bambu material-mapping RAM cache (TASKS.md Nachtrag 2026-08-28):
  // published exclusively by tasks::storageTask() via a single atomic
  // pointer store after a newly loaded/validated table is fully written
  // into one of its two PSRAM-backed double-buffer instances -- readers
  // (tasks::bambuTask()) load() it directly, never a torn/partial table.
  // Deliberately NOT carried through rtos::AppEvent: AppEvent is copied by
  // value into a 16-deep FreeRTOS queue backed by internal RAM (see
  // services/PsramAlloc.h), so every extra field costs 16x its size --
  // unaffordable for a table this size (~61 KiB with the schema-v2 rule
  // model's per-rule match buffers, up from ~18 KiB under the old flat
  // schema-v1 table; still a small fraction of the ~1.6 MiB free PSRAM
  // typically available, even double-buffered). nullptr means "no valid
  // table currently loaded" (see docs/bambu-protocol.md).
  std::atomic<const models::BambuMaterialRuleTable*> bambuMaterialMappings{
      nullptr};  ///< Currently active Bambu material-mapping rule table, or nullptr if none has loaded successfully yet.

  // Signals that tasks::storageTask() has fully finished processing a
  // StorageCommandType::CommitBambuMaterialDownload (success or failure) --
  // given exactly once per real commit by StorageTask, taken by UpdateTask
  // right after sending Commit (TASKS.md Nachtrag 2026-08-28). Needed
  // because tasks::downloadUpdate() otherwise proceeds straight to the
  // firmware download's own TLS handshake immediately after firing off the
  // material-mapping StorageCommands -- on real hardware that overlapped
  // with StorageTask still writing the SD card and reliably stalled one of
  // its writes (ESP32-S3 shares GDMA hardware between TLS/crypto
  // acceleration and SD/SPI DMA). Binary semaphore, not a queue: exactly
  // one "done" signal per download, no payload.
  SemaphoreHandle_t bambuMaterialDownloadDone = nullptr;  ///< Given by StorageTask after Commit finishes; taken by UpdateTask before proceeding to the firmware download.

  /// @brief Creates every queue, the queue set, and the event group.
  /// @return true if all objects were created successfully.
  bool createObjects();
  /// @brief Creates #loggingTask and #uiTask (must run before the service
  ///        tasks so early boot progress is visible on screen and in logs).
  /// @return true if both tasks were created successfully.
  bool createUiTask();
  /// @brief Creates every remaining task (storage, app, scale, nfc,
  ///        network, spoolman, bambu, power, update), in an order that
  ///        satisfies their startup dependencies.
  /// @return true if all tasks were created successfully.
  bool createServiceTasks();
  /// @brief Convenience wrapper calling #createUiTask() then #createServiceTasks().
  /// @return true if every task was created successfully.
  bool createTasks();
};

/// @brief The single process-wide RtosContext instance.
/// @return Reference to the global RtosContext.
RtosContext& context();
/// @brief Enqueues one already-formatted line onto the shared log queue.
/// @param message NUL-terminated line to enqueue; ignored if null.
/// @note Low-level transport used exclusively by services::Logger.
///       Application code must use FS_LOGE/W/I/D/T so every line has
///       canonical metadata.
void enqueueLogLine(const char* message);
/// @brief Number of log lines dropped because the log queue was full.
/// @return Running count since boot.
// A full logQueue silently drops the new line (see enqueueLogLine()) rather
// than blocking the producer task -- correct for a long-running device, but
// previously left with zero visibility if it ever actually happened
// (Robustheit/Diagnose, TASKS.md 10.7). Called from every task via
// FS_LOG*, so a lock-free atomic counter instead of a queue/mutex.
// Diagnostics-only; not reset except at boot.
std::uint32_t droppedLogLineCount();

}  // namespace filament_station::rtos
