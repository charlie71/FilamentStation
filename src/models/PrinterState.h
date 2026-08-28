/**
 * @file
 * @brief Per-printer/AMS/tray runtime state mirrored from Bambu Lab MQTT
 *        reports (see services::BambuProtocol, tasks::BambuTask).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace filament_station {
namespace models {

using PrinterId = std::uint16_t;  ///< Locally assigned, persisted printer identifier (see models::BambuPrinterConfig).

constexpr PrinterId kInvalidPrinterId = 0;         ///< Sentinel meaning "no printer" (never a valid assigned id).
constexpr std::size_t kMaximumPrinters = 4;         ///< Maximum number of configured printers.
constexpr std::size_t kMaximumAmsPerPrinter = 4;    ///< Maximum number of AMS units tracked per printer.
constexpr std::size_t kSlotsPerAms = 4;             ///< Number of trays per AMS unit.

// Bambu "tray_now" wire encoding (print.ams.tray_now): 0..15 address a
// global AMS slot as amsId*kSlotsPerAms + trayId (0..3 = AMS 0 slot 0..3,
// 4..7 = AMS 1, ...), 254 is the external/vt_tray spool, 255 means no tray
// is currently active. See docs/bambu-protocol.md.
constexpr std::uint8_t kActiveTrayNowExternal = 254;  ///< `tray_now` value meaning the external/manual spool holder is loaded.
constexpr std::uint8_t kActiveTrayNowNone = 255;      ///< `tray_now` value meaning no tray is currently loaded.

// Bambu "ams_filament_setting"/"extrusion_cali_sel" wire addressing for the
// external/manual spool holder (no AMS involved) -- a different pair of
// fields than tray_now above, coincidentally sharing 254 for the tray part.
// See docs/bambu-protocol.md ("Vergleich mit FilaMan-System": "Default fuer
// den externen Slot: ams_id=255, tray_id=254").
constexpr std::uint8_t kBambuExternalAmsId = 255;   ///< Wire `ams_id` value addressing the external/manual spool holder.
constexpr std::uint8_t kBambuExternalTrayId = 254;  ///< Wire `tray_id`/`slot_id` value addressing the external/manual spool holder.

/**
 * @brief MQTT connection state of one configured Bambu Lab printer.
 *
 * Only a subset of transitions is currently exercised by tasks::BambuTask:
 * `Disabled` is solely the struct's initial value (never reassigned) and
 * `Connecting` is defined but never actually set anywhere in the code
 * today -- reserved for a future in-progress indicator. The diagram below
 * shows only the transitions the code actually performs.
 *
 * @dot
 * digraph PrinterConnectionState {
 *   rankdir=LR;
 *   Disabled -> Connected [label="MQTT connect() succeeds"];
 *   Disabled -> Error [label="connect() fails"];
 *   Connected -> Offline [label="MQTT disconnect"];
 *   Offline -> Connected [label="reconnect succeeds"];
 *   Connected -> Error [label="AppEventType::BambuError"];
 * }
 * @enddot
 */
enum class PrinterConnectionState : std::uint8_t {
  Disabled,    ///< Initial value; never reassigned by the code today.
  Offline,     ///< A previously connected MQTT session was lost.
  Connecting,  ///< Reserved for a future in-progress indicator; not currently set.
  Connected,   ///< MQTT session is established and subscribed.
  Error,       ///< The connection attempt failed, or an error was reported for this printer.
};

/**
 * @brief Connection state of one AMS unit as reported by the printer.
 *
 * @note Only `Unavailable` (default, no report seen yet) and `Connected`
 *       (set whenever the unit is present in a report, see
 *       services::bambuApplyReport()) are currently produced by the code;
 *       `Offline`/`Connecting`/`Error` are defined but not yet wired up to
 *       any transition.
 */
enum class AmsConnectionState : std::uint8_t {
  Unavailable,  ///< No report has mentioned this AMS unit yet (default).
  Offline,      ///< Reserved; not currently set.
  Connecting,   ///< Reserved; not currently set.
  Connected,    ///< The unit was present in the most recent status report.
  Error,        ///< Reserved; not currently set.
};

/// @brief Occupancy state of one tray, as last reported by the printer.
///
/// @note Purely a passive mirror of the printer's own report (see
///       services::applyTrayOccupancy()) -- there is no independent
///       transition logic in this firmware; the printer decides when a
///       tray is Loading/Unloading, this enum just reflects it.
enum class PrinterSlotState : std::uint8_t {
  Unknown,    ///< No report has covered this tray yet.
  Empty,      ///< Tray reported no material loaded.
  Ready,      ///< Tray reported material loaded and ready.
  Loading,    ///< Printer reported the tray as currently loading (not currently distinguished by this firmware's report parsing).
  Loaded,     ///< Printer reported the tray as loaded (not currently distinguished by this firmware's report parsing).
  Unloading,  ///< Printer reported the tray as currently unloading (not currently distinguished by this firmware's report parsing).
  Error,      ///< Printer reported an error state for this tray (not currently distinguished by this firmware's report parsing).
};

/// @brief Last known material/color/occupancy for one physical tray slot.
struct PrinterSlotStateData {
  std::uint8_t trayId = 0;  ///< Local tray index within its AMS unit (0..kSlotsPerAms-1).
  // No spoolId field here on purpose: the printer has no notion of Spoolman
  // identities (a project-specific attempt to round-trip one through a
  // custom "tray_id_name" MQTT field was hardware-tested and abandoned, see
  // docs/bambu-protocol.md). The printer<->AMS/tray<->Spoolman-spool
  // association is tracked separately and persisted locally, see
  // models/TraySpoolCache.h.
  PrinterSlotState state = PrinterSlotState::Unknown;  ///< Last reported occupancy state.
  // Printer-reported material/color for this tray (Bambu tray_type/
  // tray_color) -- these two fields are a real Bambu identity, unrelated to
  // Spoolman, see docs/bambu-protocol.md. 16 bytes to match the outgoing
  // BambuTrayFilament::trayType/BambuTask::PendingTrayAssignment::
  // expectedTrayType capacity (services/BambuProtocol.h, tasks/
  // BambuTask.cpp) -- a smaller buffer here silently truncated long
  // tray_type values (e.g. "Support for PLA"), which then never matched
  // the untruncated expected value and made AssignTray's confirmation
  // (BambuTask.cpp::checkPendingTrayAssignment()) time out forever, even
  // though the printer applied the assignment correctly.
  char material[16]{};   ///< Printer-reported `tray_type`.
  char colorHex[9]{};    ///< Printer-reported `tray_color` (8-digit RRGGBBAA hex).
  // Printer-reported nozzle_temp_min/max for this tray, if the report that
  // last touched this slot included them (see
  // services::applyTrayOccupancy()) -- not every report does, so these are
  // 0 ("unknown") until a report with the fields arrives, and keep their
  // last known value otherwise. AMS-slot metadata only, from
  // services::BambuMaterialMapping; unrelated to the actual print
  // temperature, which always comes from the slicer/filament profile.
  std::uint16_t nozzleTempMinC = 0;  ///< Printer-reported minimum nozzle temperature for this tray, or 0 if never reported.
  std::uint16_t nozzleTempMaxC = 0;  ///< Printer-reported maximum nozzle temperature for this tray, or 0 if never reported.
};

/// @brief Last known state of one AMS unit and its four trays.
struct AmsState {
  std::uint8_t amsId = 0;                    ///< 0-based AMS unit index.
  bool present = false;                      ///< Whether this unit has appeared in a status report.
  AmsConnectionState connectionState = AmsConnectionState::Unavailable;  ///< Connection state of this unit.
  std::array<PrinterSlotStateData, kSlotsPerAms> slots{};  ///< The unit's four trays.
};

/// @brief Full runtime state of one configured printer: connection,
///        identity, and every AMS unit/tray/external slot.
struct PrinterState {
  PrinterId printerId = kInvalidPrinterId;  ///< Locally assigned printer id.
  char name[32]{};                          ///< Display name.
  bool enabled = false;                     ///< Whether this printer is enabled for connection attempts.
  bool isDefault = false;                   ///< Whether this is the printer selected on a fresh boot.
  bool isActive = false;                    ///< Whether this printer is currently shown/focused in the UI.
  PrinterConnectionState connectionState = PrinterConnectionState::Disabled;  ///< Current MQTT connection state.
  std::uint8_t activeAmsId = 0;             ///< AMS unit currently shown on the Home/AMS overview.
  std::uint8_t amsCount = 0;                ///< Number of AMS units reported present.
  std::array<AmsState, kMaximumAmsPerPrinter> amsUnits{};  ///< State of every AMS unit.
  PrinterSlotStateData externalSlot{};       ///< State of the external/manual spool holder (no AMS).
  // Printer-reported nozzle diameter (e.g. "0.4"), from "print.nozzle_diameter"
  // in status reports. Empty until the first report with that field arrives;
  // required (as a string) by the "extrusion_cali_sel" wire command, see
  // docs/bambu-protocol.md.
  char nozzleDiameter[8]{};  ///< Printer-reported nozzle diameter string, e.g. "0.4".
  // Which tray is currently loaded into the nozzle ("print.ams.tray_now"),
  // see kActiveTrayNowExternal/kActiveTrayNowNone above -- Nutzerwunsch
  // 2026-08-24 (Home-Tray-Karten zeigen das Duesen-Icon nur beim wirklich
  // aktiven Fach statt einer Mockformel). Kept at its last known value if a
  // report doesn't include the field, same merge behavior as the rest of
  // this struct.
  std::uint8_t activeTrayNow = kActiveTrayNowNone;  ///< Global tray index currently loaded into the nozzle, or kActiveTrayNowExternal/kActiveTrayNowNone.
};

/// @brief All configured printers plus which one is active/default.
struct PrinterStateCollection {
  std::array<PrinterState, kMaximumPrinters> printers{};  ///< Configured printers.
  std::uint8_t printerCount = 0;         ///< Number of valid entries in #printers.
  PrinterId activePrinterId = kInvalidPrinterId;   ///< Printer currently focused in the UI.
  PrinterId defaultPrinterId = kInvalidPrinterId;  ///< Printer selected on a fresh boot.
};

/// @brief Whether a PrinterId refers to a real, assigned printer.
/// @param printerId The id to check.
/// @return True unless printerId equals kInvalidPrinterId.
constexpr bool isValidPrinterId(PrinterId printerId) {
  return printerId != kInvalidPrinterId;
}

/// @brief Finds a printer by id.
/// @param collection Collection to search.
/// @param printerId Id to look up.
/// @return Pointer to the matching entry, or nullptr if not found/invalid id.
inline PrinterState* findPrinter(PrinterStateCollection& collection,
                                 PrinterId printerId) {
  if (!isValidPrinterId(printerId)) return nullptr;
  const std::size_t count = collection.printerCount < kMaximumPrinters
                                ? collection.printerCount
                                : kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index) {
    if (collection.printers[index].printerId == printerId)
      return &collection.printers[index];
  }
  return nullptr;
}

/// @brief Const overload of findPrinter().
/// @param collection Collection to search.
/// @param printerId Id to look up.
/// @return Pointer to the matching entry, or nullptr if not found/invalid id.
inline const PrinterState* findPrinter(
    const PrinterStateCollection& collection, PrinterId printerId) {
  if (!isValidPrinterId(printerId)) return nullptr;
  const std::size_t count = collection.printerCount < kMaximumPrinters
                                ? collection.printerCount
                                : kMaximumPrinters;
  for (std::size_t index = 0; index < count; ++index) {
    if (collection.printers[index].printerId == printerId)
      return &collection.printers[index];
  }
  return nullptr;
}

/// @brief Finds an AMS unit by id within one printer.
/// @param printer Printer to search.
/// @param amsId AMS unit index to look up.
/// @return Pointer to the matching, present unit, or nullptr if not found.
inline AmsState* findAms(PrinterState& printer, std::uint8_t amsId) {
  for (auto& ams : printer.amsUnits) {
    if (ams.present && ams.amsId == amsId) return &ams;
  }
  return nullptr;
}

/// @brief Const overload of findAms().
/// @param printer Printer to search.
/// @param amsId AMS unit index to look up.
/// @return Pointer to the matching, present unit, or nullptr if not found.
inline const AmsState* findAms(const PrinterState& printer,
                              std::uint8_t amsId) {
  for (const auto& ams : printer.amsUnits) {
    if (ams.present && ams.amsId == amsId) return &ams;
  }
  return nullptr;
}

/// @brief Finds one tray's state within a printer's AMS units.
/// @param printer Printer to search.
/// @param amsId AMS unit index.
/// @param trayId Tray index within that unit.
/// @return Pointer to the tray's state, or nullptr if the AMS unit isn't
///         present or trayId is out of range.
inline PrinterSlotStateData* findSlot(PrinterState& printer,
                                      std::uint8_t amsId,
                                      std::uint8_t trayId) {
  AmsState* ams = findAms(printer, amsId);
  if (ams == nullptr || trayId >= kSlotsPerAms) return nullptr;
  return &ams->slots[trayId];
}

/// @brief Validates that every printer in a collection has a distinct id.
/// @param collection Collection to check.
/// @return True if printerCount is within bounds and every id is unique and valid.
inline bool hasUniquePrinterIds(const PrinterStateCollection& collection) {
  if (collection.printerCount > kMaximumPrinters) return false;
  for (std::size_t left = 0; left < collection.printerCount; ++left) {
    const PrinterId id = collection.printers[left].printerId;
    if (!isValidPrinterId(id)) return false;
    for (std::size_t right = left + 1; right < collection.printerCount;
         ++right) {
      if (collection.printers[right].printerId == id) return false;
    }
  }
  return true;
}

/// @brief Validates internal consistency of a whole PrinterStateCollection
///        (unique ids, exactly one active/default printer, valid AMS
///        references).
/// @param collection Collection to check.
/// @return True if the collection is internally consistent.
inline bool isValidPrinterState(const PrinterStateCollection& collection) {
  if (!hasUniquePrinterIds(collection)) return false;
  if (collection.printerCount == 0)
    return collection.activePrinterId == kInvalidPrinterId &&
           collection.defaultPrinterId == kInvalidPrinterId;

  const PrinterState* active = findPrinter(collection, collection.activePrinterId);
  const PrinterState* defaultPrinter =
      findPrinter(collection, collection.defaultPrinterId);
  if (active == nullptr || defaultPrinter == nullptr || !active->isActive ||
      !defaultPrinter->isDefault)
    return false;

  std::size_t activeCount = 0;
  std::size_t defaultCount = 0;
  for (std::size_t index = 0; index < collection.printerCount; ++index) {
    const PrinterState& printer = collection.printers[index];
    if (printer.amsCount > kMaximumAmsPerPrinter) return false;
    if (printer.isActive) ++activeCount;
    if (printer.isDefault) ++defaultCount;
    if (printer.amsCount > 0 && findAms(printer, printer.activeAmsId) == nullptr)
      return false;
  }
  return activeCount == 1 && defaultCount == 1;
}

static_assert(std::is_trivially_copyable<PrinterSlotStateData>::value,
              "PrinterSlotStateData must be trivially copyable");
static_assert(std::is_trivially_copyable<AmsState>::value,
              "AmsState must be trivially copyable");
static_assert(std::is_trivially_copyable<PrinterState>::value,
              "PrinterState must be trivially copyable");
static_assert(std::is_trivially_copyable<PrinterStateCollection>::value,
              "PrinterStateCollection must be trivially copyable");

}  // namespace models
}  // namespace filament_station
