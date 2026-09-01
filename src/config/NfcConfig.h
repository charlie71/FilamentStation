/**
 * @file
 * @brief Timing, retry, and buffer-size constants for the PN532/NTAG21x
 *        NFC pipeline (tasks::nfcTask()).
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace config {

constexpr std::uint32_t kNfcScanIntervalMs = 250;  ///< Polling interval NfcTask uses between RF scan attempts.
// A complete NTAG215/216 analysis can involve many RF transactions. Allow
// transient PN532 re-selection failures without reporting a false removal.
constexpr std::uint32_t kNfcTagRemovalDelayMs = 1000;  ///< Grace period after a re-selection failure before a tag is treated as removed.
// A single failed re-selection is common after a longer NTAG transaction.
// Require several independent RF scans before declaring physical removal.
constexpr std::uint8_t kNfcRemovalConfirmationScans = 3;  ///< Consecutive failed scans required to confirm tag removal.
// After the selected PN532 target has been lost, require several complete RF
// re-activations which explicitly report no target.  A failed transaction is
// not proof of physical removal, especially with NTAG215/216.
constexpr std::uint8_t kNfcFreshAbsenceConfirmationScans = 5;  ///< Consecutive explicit "no target" scans required after a lost selection before absence is confirmed.
constexpr std::uint32_t kPn532ResponseTimeoutMs = 500;  ///< Timeout for a single PN532 command/response transceive.
constexpr std::uint8_t kPn532MaxTargetRetries = 0x02;   ///< PN532 InListPassiveTarget retry count.
// A handful of communication errors in a row already triggers a soft RF
// reset (see NfcTask.cpp); only a much longer *sustained* failure run,
// spanning several such reset attempts, is treated as the PN532 itself
// being disconnected (Robustheit/Diagnose-Nachtrag, TASKS.md 10.2) -- a
// single bad UART frame or a lengthy NTAG transaction must not falsely
// report the reader as gone.
constexpr std::uint16_t kPn532DisconnectConfirmationScans = 20;  ///< Sustained failed-scan count treated as the PN532 module itself being disconnected.
// NXP specifies up to 10 ms for an NTAG21x WRITE operation. Do not start the
// next page transaction while the tag can still be programming its EEPROM.
constexpr std::uint32_t kNtagPageWriteSettleMs = 10;      ///< Delay after an NTAG21x page WRITE before issuing the next command.
constexpr std::uint8_t kNtagPageWriteAttempts = 3;        ///< Retry count for a single NTAG21x page write.
constexpr std::uint8_t kNtagVerificationScanAttempts = 3;  ///< Retry count for the post-write/erase re-read verification scan.
constexpr std::size_t kNfcMaxUidLength = 10;              ///< Largest UID length (bytes) the raw-tag buffers accommodate.
// Der offizielle OpenPrintTag-MK1-Testvektor belegt 312 Byte Tag-Speicher.
constexpr std::size_t kNfcMaxNdefBytes = 384;  ///< Largest raw NDEF payload the tag-read buffers accommodate.

}  // namespace config
}  // namespace filament_station
