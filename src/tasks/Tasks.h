/**
 * @file
 * @brief Entry points for every FreeRTOS task, created by
 *        rtos::RtosContext::createTasks(). See docs/architecture.md.
 */
#pragma once

namespace filament_station::tasks {
/// @brief LVGL UI task: owns the display/touch driver and renders every rtos::UiCommand.
/// @param parameter Pointer to the owning rtos::RtosContext.
void uiTask(void* parameter);
/// @brief Central application-control task: owns all persistent app state and orchestrates every other task.
/// @param parameter Pointer to the owning rtos::RtosContext.
void appTask(void* parameter);
/// @brief HX711 load-cell task: reads, filters, and reports scale measurements.
/// @param parameter Pointer to the owning rtos::RtosContext.
void scaleTask(void* parameter);
/// @brief PN532 NFC task: detects, reads, and writes tags.
/// @param parameter Pointer to the owning rtos::RtosContext.
void nfcTask(void* parameter);
/// @brief SD-card task: owns all JSON document read/write/atomic-save operations.
/// @param parameter Pointer to the owning rtos::RtosContext.
void storageTask(void* parameter);
/// @brief WiFi/network task: manages station connection and the config portal.
/// @param parameter Pointer to the owning rtos::RtosContext.
void networkTask(void* parameter);
/// @brief Spoolman HTTP client task: performs all Spoolman API requests.
/// @param parameter Pointer to the owning rtos::RtosContext.
void spoolmanTask(void* parameter);
/// @brief Bambu Lab MQTT client task: manages printer connections and commands.
/// @param parameter Pointer to the owning rtos::RtosContext.
void bambuTask(void* parameter);
/// @brief Energy-saving task: drives the Active/Dimmed/Sleep power state machine.
/// @param parameter Pointer to the owning rtos::RtosContext.
void powerTask(void* parameter);
/// @brief Firmware-update task: checks for and downloads new firmware versions.
/// @param parameter Pointer to the owning rtos::RtosContext.
void updateTask(void* parameter);
}  // namespace filament_station::tasks
