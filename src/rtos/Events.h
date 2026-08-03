#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace filament_station::rtos {

enum class AppEventType : std::uint8_t {
  UiCommunicationTest,
  ScaleReady,
  ScaleStable,
  ScaleError,
  NfcTagDetected,
  NfcTagRead,
  NfcTagWritten,
  NfcError,
  SdMounted,
  SdRemoved,
  SdReinserted,
  SdError,
  WifiConnected,
  WifiDisconnected,
  WifiConfigPortalStarted,
  SpoolmanConnected,
  SpoolmanResponse,
  SpoolmanError,
  BambuConnected,
  BambuUpdate,
  BambuError
};

constexpr EventBits_t EVENT_UI_READY = BIT0;
constexpr EventBits_t EVENT_SD_READY = BIT1;
constexpr EventBits_t EVENT_SCALE_READY = BIT2;
constexpr EventBits_t EVENT_NFC_READY = BIT3;
constexpr EventBits_t EVENT_WIFI_CONNECTED = BIT4;
constexpr EventBits_t EVENT_SPOOLMAN_READY = BIT5;
constexpr EventBits_t EVENT_BAMBU_READY = BIT6;
constexpr EventBits_t EVENT_FATAL_ERROR = BIT7;

}  // namespace filament_station::rtos
