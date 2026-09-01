/**
 * @file
 * @brief Fixed hardware wiring and low-level peripheral timing constants
 *        for the WT32-SC01-Plus (ESP32-S3 + ST7796UI + FT6336U) board.
 *
 * See docs/hardware.md for the full pin-conflict analysis and datasheet
 * references behind these values.
 */
#pragma once

#include <cstdint>

namespace filament_station::config {

constexpr std::uint32_t kSerialBaudRate = 115200;              ///< Baud rate for the native USB-CDC serial console.
constexpr std::uint16_t kDisplayWidth = 480;                   ///< Logical (rotated) display width in pixels.
constexpr std::uint16_t kDisplayHeight = 320;                  ///< Logical (rotated) display height in pixels.
constexpr std::uint16_t kDisplayNativeWidth = 320;              ///< Physical panel width before rotation.
constexpr std::uint16_t kDisplayNativeHeight = 480;             ///< Physical panel height before rotation.
constexpr std::uint8_t kDisplayRotation = 3;                    ///< LovyanGFX rotation value (3 = 180 degrees, matches the enclosure orientation).
constexpr std::uint32_t kDisplayWriteFrequencyHz = 20000000;    ///< MCU8080 parallel bus write clock.
constexpr std::uint32_t kDisplayBacklightPwmFrequencyHz = 44100;  ///< Backlight PWM carrier frequency.
constexpr std::uint8_t kDisplayBacklightPwmChannel = 7;         ///< LEDC channel used for the backlight PWM signal.
constexpr std::uint8_t kDisplayDefaultBrightness = 255;         ///< Backlight duty cycle (0-255) while PowerTask considers the device Active.

// WT32-SC01-Plus LCD: ST7796UI, 8-bit MCU8080 parallel interface.
constexpr std::int8_t kDisplayBacklightPin = 45;       ///< Backlight PWM output pin.
constexpr bool kDisplayBacklightActiveHigh = true;     ///< Backlight polarity.
constexpr std::int8_t kDisplayResetPin = 4;            ///< Shared LCD/touch hardware reset pin.
constexpr std::int8_t kDisplayCommandDataPin = 0;      ///< LCD RS/D-C pin (also the BOOT strapping pin).
constexpr std::int8_t kDisplayWritePin = 47;           ///< MCU8080 write-strobe pin.
constexpr std::int8_t kDisplayTearingEffectPin = 48;   ///< Optional frame-sync (TE) pin, currently unused.
constexpr std::int8_t kDisplayDataPins[8] = {9, 46, 3, 8, 18, 17, 16, 15};  ///< MCU8080 data bus, DB0..DB7 in order.

// WT32-SC01-Plus touch: FT6336U on I2C. Reset is shared with LCD.
constexpr std::int8_t kTouchInterruptPin = 7;   ///< FT6336U INT pin; wired but not used as a hardware interrupt (see docs/rtos.md).
constexpr std::int8_t kTouchSdaPin = 6;         ///< Touch I2C data pin.
constexpr std::int8_t kTouchSclPin = 5;         ///< Touch I2C clock pin.
constexpr std::int8_t kTouchResetPin = kDisplayResetPin;  ///< Touch reset, physically shared with the LCD reset.
constexpr std::uint8_t kTouchI2cAddress = 0x38;  ///< FT6336U I2C address.
constexpr std::uint32_t kTouchI2cFrequencyHz = 400000;  ///< Touch I2C bus clock.

// Bestaetigte SPI-SD-Pinbelegung des WT32-SC01-Plus.
constexpr std::int8_t kSdChipSelectPin = 41;   ///< SD card SPI chip-select pin.
constexpr std::int8_t kSdMosiPin = 40;         ///< SD card SPI MOSI/DI pin.
constexpr std::int8_t kSdClockPin = 39;        ///< SD card SPI clock pin.
constexpr std::int8_t kSdMisoPin = 38;         ///< SD card SPI MISO/DO pin.
constexpr bool kSdCardDetectAvailable = false;  ///< No physical card-detect signal is wired on this board.

// Ohne Card-Detect ist ein langsamer Task-Fallback erforderlich. Nach einer
// Entfernung bleibt der Storage-Fehler bis zum Neustart verriegelt.
constexpr std::uint32_t kSdHealthCheckIntervalMs = 2000;  ///< Polling interval StorageTask uses to detect card removal in the absence of a card-detect pin.

// Eine einzelne Kommandobearbeitung (Laden/Speichern/Loeschen) laenger als
// dieser Schwellwert deutet auf eine degradierende/langsame SD-Karte hin
// (Robustheit/Diagnose, TASKS.md 10.2) -- kein Abbruch, nur Diagnose-
// Sichtbarkeit im Log, da die eigentliche Blockierung ohnehin auf den
// dedizierten StorageTask begrenzt bleibt (kein anderer Task wartet
// synchron auf SD-I/O).
constexpr std::uint32_t kSdSlowOperationWarningMs = 750;  ///< Diagnostic-only threshold above which a single SD operation is logged as slow.

// HX711 am herausgefuehrten EXT-Anschluss des WT32-SC01-Plus:
// EXT_IO2/GPIO11 ist ein interruptfaehiger Eingang fuer DOUT,
// EXT_IO1/GPIO10 ist der Taktausgang. Beide Pins sind laut Boarddatenblatt
// frei herausgefuehrt und kollidieren nicht mit Display, Touch oder SD.
constexpr std::int8_t kHx711DataPin = 11;    ///< HX711 DOUT pin (interrupt-capable, falling-edge data-ready signal).
constexpr std::int8_t kHx711ClockPin = 10;   ///< HX711 SCK (clock) output pin.
constexpr std::uint32_t kHx711ReadyTimeoutMs = 1500;  ///< Time without a DOUT falling edge before ScaleTask reports a scale error.

// PN532 ueber HSU/UART am herausgefuehrten EXT-Anschluss:
// Zuordnung fuer die vorhandene, bereits signalgekreuzte Verkabelung:
// ESP32 TX/GPIO12 -> PN532 RX, ESP32 RX/GPIO13 <- PN532 TX.
// GPIO12 und GPIO13 sind laut Boarddatenblatt freie EXT-I/Os und kollidieren
// nicht mit Display, Touch, SD, USB oder HX711.
constexpr std::int8_t kPn532UartNumber = 1;              ///< ESP32 hardware UART peripheral number used for the PN532.
constexpr std::int8_t kPn532UartTxPin = 12;              ///< ESP32 UART TX pin, wired to the PN532's RX.
constexpr std::int8_t kPn532UartRxPin = 13;              ///< ESP32 UART RX pin, wired to the PN532's TX.
constexpr std::uint32_t kPn532UartBaudRate = 115200;      ///< PN532 HSU baud rate.
constexpr std::size_t kPn532UartRxBufferSize = 256;       ///< Arduino UART driver receive-buffer size.
constexpr std::uint8_t kPn532UartEventQueueLength = 8;    ///< Arduino UART driver internal event-queue length.
constexpr bool kPn532UsesExternalIrq = false;             ///< No PN532 IRQ pin is wired; responses are detected via the UART driver's own interrupt-driven receive.

}  // namespace filament_station::config
