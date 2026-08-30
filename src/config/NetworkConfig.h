/**
 * @file
 * @brief WiFiManager captive-portal and reconnect-loop tuning constants
 *        (tasks::networkTask()).
 */
#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr char kWifiPortalPasswordPrefix[] = "FS";       ///< Prefix for the auto-generated captive-portal WiFi password (see docs/user-guide.md).
constexpr std::uint32_t kWifiPortalServiceIntervalMs = 20;  ///< How often NetworkTask services the WiFiManager captive-portal loop.
constexpr std::uint8_t kWifiEventQueueLength = 8;         ///< Length of the internal WiFi-event queue merged into networkQueueSet.
// WiFiManager's own ESP32 auto-reconnect (WiFi_autoReconnect()) is gated
// behind the "esp32autoreconnect" build macro, which this project never
// defines -- confirmed by reading its vendored source
// (.pio/libdeps/.../WiFiManager/WiFiManager.cpp). Without it, nothing ever
// retries after a runtime WiFi loss (router reboot, signal drop, etc.); the
// device stays disconnected until a manual reboot (Robustheit/Diagnose,
// TASKS.md 10.4). NetworkTask.cpp instead retries WiFi.reconnect() itself on
// this interval whenever configured-but-disconnected and no captive portal
// is active -- long enough to not hammer the radio/AP on a genuine outage,
// short enough that a brief outage recovers well within a user's notice.
constexpr std::uint32_t kWifiReconnectIntervalMs = 15000;  ///< Interval between NetworkTask's own WiFi.reconnect() retries after a runtime connection loss.
// Nutzerbericht 2026-08-30: nach dem Aufwachen aus dem Energiesparmodus
// (WiFi.mode(WIFI_OFF) beim Einschlafen, WiFi.mode(WIFI_STA) beim
// Aufwachen) schlägt der erste WiFi.reconnect()-Versuch vereinzelt fehl,
// klappt beim nächsten Aufwachen aber wieder -- ein bekanntes ESP32-
// Arduino-Verhalten: WiFi.mode(WIFI_STA) kehrt zurück, bevor die
// STA-Schnittstelle (Funkkalibrierung) tatsächlich vollständig bereit
// ist, sodass ein sofort im selben Tick folgender reconnect()-Aufruf
// gelegentlich zu früh kommt. Ein erster Fix (fester 300-ms-Delay vor dem
// ersten Versuch) löste das Problem laut Nutzerrückmeldung nicht
// zuverlässig -- ersetzt durch ein echtes, auf ARDUINO_EVENT_WIFI_STA_START
// wartendes Verfahren (siehe NetworkTask.cpp's awaitingStaStart), das sich
// an der tatsächlichen Hardware-Bereitschaft orientiert statt an einer
// geratenen festen Zeit. Diese beiden Konstanten steuern nur noch dessen
// Poll-Intervall bzw. Fallback-Timeout, falls das Ereignis auf dieser
// Hardware/Firmware-Version doch einmal ausbleiben sollte -- weiterhin
// unverifiziert (kein Hardware-Test in dieser Sitzung möglich).
constexpr std::uint32_t kWifiStaStartPollIntervalMs = 50;  ///< How often NetworkTask polls while waiting for ARDUINO_EVENT_WIFI_STA_START after waking.
constexpr std::uint32_t kWifiStaStartFallbackMs = 2000;    ///< Maximum time to wait for ARDUINO_EVENT_WIFI_STA_START before attempting WiFi.reconnect() anyway.

}  // namespace filament_station::config
