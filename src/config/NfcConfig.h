#pragma once

#include <cstddef>
#include <cstdint>

namespace filament_station {
namespace config {

constexpr std::uint32_t kNfcScanIntervalMs = 250;
// A complete NTAG215/216 analysis can involve many RF transactions. Allow
// transient PN532 re-selection failures without reporting a false removal.
constexpr std::uint32_t kNfcTagRemovalDelayMs = 1000;
// A single failed re-selection is common after a longer NTAG transaction.
// Require several independent RF scans before declaring physical removal.
constexpr std::uint8_t kNfcRemovalConfirmationScans = 3;
// After the selected PN532 target has been lost, require several complete RF
// re-activations which explicitly report no target.  A failed transaction is
// not proof of physical removal, especially with NTAG215/216.
constexpr std::uint8_t kNfcFreshAbsenceConfirmationScans = 5;
constexpr std::uint32_t kPn532ResponseTimeoutMs = 500;
constexpr std::uint8_t kPn532MaxTargetRetries = 0x02;
// A handful of communication errors in a row already triggers a soft RF
// reset (see NfcTask.cpp); only a much longer *sustained* failure run,
// spanning several such reset attempts, is treated as the PN532 itself
// being disconnected (Robustheit/Diagnose-Nachtrag, TASKS.md 10.2) -- a
// single bad UART frame or a lengthy NTAG transaction must not falsely
// report the reader as gone.
constexpr std::uint16_t kPn532DisconnectConfirmationScans = 20;
// NXP specifies up to 10 ms for an NTAG21x WRITE operation. Do not start the
// next page transaction while the tag can still be programming its EEPROM.
constexpr std::uint32_t kNtagPageWriteSettleMs = 10;
constexpr std::uint8_t kNtagPageWriteAttempts = 3;
constexpr std::uint8_t kNtagVerificationScanAttempts = 3;
constexpr std::size_t kNfcMaxUidLength = 10;
// Der offizielle OpenPrintTag-MK1-Testvektor belegt 312 Byte Tag-Speicher.
constexpr std::size_t kNfcMaxNdefBytes = 384;

}  // namespace config
}  // namespace filament_station
