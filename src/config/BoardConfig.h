#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr std::uint32_t kSerialBaudRate = 115200;
constexpr std::uint16_t kDisplayWidth = 480;
constexpr std::uint16_t kDisplayHeight = 320;
constexpr std::uint16_t kDisplayNativeWidth = 320;
constexpr std::uint16_t kDisplayNativeHeight = 480;
constexpr std::uint8_t kDisplayRotation = 3;
constexpr std::uint32_t kDisplayWriteFrequencyHz = 20000000;
constexpr std::uint32_t kDisplayBacklightPwmFrequencyHz = 44100;
constexpr std::uint8_t kDisplayBacklightPwmChannel = 7;
constexpr std::uint8_t kDisplayDefaultBrightness = 192;

// WT32-SC01-Plus LCD: ST7796UI, 8-bit MCU8080 parallel interface.
constexpr std::int8_t kDisplayBacklightPin = 45;
constexpr bool kDisplayBacklightActiveHigh = true;
constexpr std::int8_t kDisplayResetPin = 4;
constexpr std::int8_t kDisplayCommandDataPin = 0;
constexpr std::int8_t kDisplayWritePin = 47;
constexpr std::int8_t kDisplayTearingEffectPin = 48;
constexpr std::int8_t kDisplayDataPins[8] = {9, 46, 3, 8, 18, 17, 16, 15};

// WT32-SC01-Plus touch: FT6336U on I2C. Reset is shared with LCD.
constexpr std::int8_t kTouchInterruptPin = 7;
constexpr std::int8_t kTouchSdaPin = 6;
constexpr std::int8_t kTouchSclPin = 5;
constexpr std::int8_t kTouchResetPin = kDisplayResetPin;
constexpr std::uint8_t kTouchI2cAddress = 0x38;
constexpr std::uint32_t kTouchI2cFrequencyHz = 400000;

// Bestaetigte SPI-SD-Pinbelegung des WT32-SC01-Plus.
constexpr std::int8_t kSdChipSelectPin = 41;
constexpr std::int8_t kSdMosiPin = 40;
constexpr std::int8_t kSdClockPin = 39;
constexpr std::int8_t kSdMisoPin = 38;
constexpr bool kSdCardDetectAvailable = false;

// Ohne Card-Detect ist ein langsamer Task-Fallback erforderlich. Nach einer
// Entfernung bleibt der Storage-Fehler bis zum Neustart verriegelt.
constexpr std::uint32_t kSdHealthCheckIntervalMs = 2000;

// Eine einzelne Kommandobearbeitung (Laden/Speichern/Loeschen) laenger als
// dieser Schwellwert deutet auf eine degradierende/langsame SD-Karte hin
// (Robustheit/Diagnose, TASKS.md 10.2) -- kein Abbruch, nur Diagnose-
// Sichtbarkeit im Log, da die eigentliche Blockierung ohnehin auf den
// dedizierten StorageTask begrenzt bleibt (kein anderer Task wartet
// synchron auf SD-I/O).
constexpr std::uint32_t kSdSlowOperationWarningMs = 750;

// HX711 am herausgefuehrten EXT-Anschluss des WT32-SC01-Plus:
// EXT_IO2/GPIO11 ist ein interruptfaehiger Eingang fuer DOUT,
// EXT_IO1/GPIO10 ist der Taktausgang. Beide Pins sind laut Boarddatenblatt
// frei herausgefuehrt und kollidieren nicht mit Display, Touch oder SD.
constexpr std::int8_t kHx711DataPin = 11;
constexpr std::int8_t kHx711ClockPin = 10;
constexpr std::uint32_t kHx711ReadyTimeoutMs = 1500;

// PN532 ueber HSU/UART am herausgefuehrten EXT-Anschluss:
// Zuordnung fuer die vorhandene, bereits signalgekreuzte Verkabelung:
// ESP32 TX/GPIO12 -> PN532 RX, ESP32 RX/GPIO13 <- PN532 TX.
// GPIO12 und GPIO13 sind laut Boarddatenblatt freie EXT-I/Os und kollidieren
// nicht mit Display, Touch, SD, USB oder HX711.
constexpr std::int8_t kPn532UartNumber = 1;
constexpr std::int8_t kPn532UartTxPin = 12;
constexpr std::int8_t kPn532UartRxPin = 13;
constexpr std::uint32_t kPn532UartBaudRate = 115200;
constexpr std::size_t kPn532UartRxBufferSize = 256;
constexpr std::uint8_t kPn532UartEventQueueLength = 8;
constexpr bool kPn532UsesExternalIrq = false;

}  // namespace filament_station::config
