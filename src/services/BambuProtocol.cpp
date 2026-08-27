/**
 * @file
 * @brief Implements the Bambu LAN-Mode MQTT protocol encode/decode helpers
 *        declared in services/BambuProtocol.h.
 */
#include "services/BambuProtocol.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace filament_station {
namespace services {
namespace {

/// @brief Case-insensitive "does material start with prefix" check.
/// @param material Free-text material name to test (e.g. "PLA Basic").
/// @param prefix Material family prefix to match (e.g. "PLA").
/// @return true if `material` starts with `prefix`, ignoring case.
// Spoolman material strings are free text (e.g. "PLA", "PLA Basic",
// "PETG-CF"), so this matches on the material family prefix rather than
// requiring an exact string.
bool materialStartsWithCaseInsensitive(const char* material,
                                       const char* prefix) {
  std::size_t index = 0;
  for (; prefix[index] != '\0'; ++index) {
    if (material[index] == '\0') return false;
    if (std::toupper(static_cast<unsigned char>(material[index])) !=
        std::toupper(static_cast<unsigned char>(prefix[index])))
      return false;
  }
  return true;
}

/// @brief Parses a tray/AMS index that may be encoded as a JSON number or
///        as text, rejecting anything at/above `maxExclusive`.
/// @param value JSON value to parse.
/// @param maxExclusive Exclusive upper bound on the accepted index.
/// @param out Out parameter receiving the parsed index.
/// @return false if `value` is not a number/numeric string, or is out of range.
bool parseTrayIndex(JsonVariantConst value, std::uint8_t maxExclusive,
                    std::uint8_t& out) {
  if (value.is<std::uint8_t>()) {
    const auto parsed = value.as<std::uint8_t>();
    if (parsed >= maxExclusive) return false;
    out = parsed;
    return true;
  }
  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed >= maxExclusive)
      return false;
    out = static_cast<std::uint8_t>(parsed);
    return true;
  }
  return false;
}

/// @brief Parses "tray_now", whose valid range is the full 0..255
///        (254/255 are sentinels, see models::kActiveTrayNowExternal/
///        kActiveTrayNowNone) -- doesn't fit parseTrayIndex()'s uint8_t
///        maxExclusive bound, hence a separate function.
/// @param value JSON value to parse.
/// @param out Out parameter receiving the parsed index.
/// @return false if `value` is not a number/numeric string, or out of the uint8_t range.
bool parseTrayNow(JsonVariantConst value, std::uint8_t& out) {
  if (value.is<std::uint8_t>()) {
    out = value.as<std::uint8_t>();
    return true;
  }
  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 255) return false;
    out = static_cast<std::uint8_t>(parsed);
    return true;
  }
  return false;
}

/// @brief One (material family prefix, generic profile id) mapping entry.
struct GenericMaterialMapping {
  const char* material;     ///< Material family prefix, matched case-insensitively (see materialStartsWithCaseInsensitive()).
  const char* trayInfoIdx;  ///< Bambu generic filament profile "setting_id" for this material.
};

/// @brief Bambu Studio's built-in *generic* (non-brand) filament profile ids.
// community-documented via Bambu-Research-Group/RFID-Tag-Guide and the
// WolfWithSword Home Assistant Bambu Lab integration (see
// docs/bambu-protocol.md) -- not from Bambu Lab directly. Composite
// materials ("-CF" carbon-fiber variants) are listed before their plain
// base material so the longer prefix matches first.
constexpr GenericMaterialMapping kGenericMaterialMappings[] = {
    {"PLA-CF", "GFL98"}, //Generic PLA CF
    {"PLACF", "GFL98"},
    {"PA-CF", "GFN98"}, 
    {"PACF", "GFN98"},
    {"PLA", "GFL99"}, //Generic PLA
    {"PETG", "GFG99"},//Generic PETG
    {"ASA", "GFB98"}, //Generic ASA
    {"ABS", "GFB99"}, //Generic ABS
    {"TPU", "GFU99"}, //Generic TPU
    {"PVA", "GFS99"}, //Generic Support
    {"PC", "GFC99"}, //Generic PC
    {"PA", "GFN99"}, //Generic PA (Nylon)
    
};

/// @brief Merges one tray's occupancy/material/color fields from a status
///        report into a slot's state.
/// @param trayJson The tray's JSON object from the report (an AMS tray or "vt_tray").
/// @param slot Slot state to update in place.
/// @note `slot.spoolId` is deliberately never touched here -- see
///       bambuApplyReport()'s doc comment.
void applyTrayOccupancy(JsonObjectConst trayJson,
                        models::PrinterSlotStateData& slot) {
  const char* trayType = trayJson["tray_type"] | "";
  slot.state = trayType[0] != '\0' ? models::PrinterSlotState::Ready
                                   : models::PrinterSlotState::Empty;
  std::snprintf(slot.material, sizeof(slot.material), "%s", trayType);
  const char* trayColor = trayJson["tray_color"] | "";
  std::snprintf(slot.colorHex, sizeof(slot.colorHex), "%s", trayColor);
  // spoolId is deliberately never touched here -- see bambuApplyReport()'s
  // doc comment and docs/bambu-protocol.md: hardware-confirmed
  // (2026-08-23) that this printer accepts "tray_id_name" on write but
  // echoes it back empty on every subsequent report, not just after a
  // reconnect. Parsing it back here previously wiped a just-confirmed
  // spoolId within seconds, same session included.
}

}  // namespace

void bambuReportTopic(const char* serialNumber, char* output,
                      std::size_t outputCapacity) {
  std::snprintf(output, outputCapacity, "device/%s/report", serialNumber);
}

void bambuRequestTopic(const char* serialNumber, char* output,
                       std::size_t outputCapacity) {
  std::snprintf(output, outputCapacity, "device/%s/request", serialNumber);
}

void bambuNormalizeTrayColorHex(const char* input, char* output) {
  std::snprintf(output, 9, "%s", input != nullptr ? input : "");
  if (std::strlen(output) == 6) {
    output[6] = 'F';
    output[7] = 'F';
    output[8] = '\0';
  }
}

std::size_t bambuBuildPushAllRequest(std::uint32_t sequenceId, char* output,
                                     std::size_t outputCapacity) {
  const int written = std::snprintf(
      output, outputCapacity,
      R"({"pushing":{"sequence_id":"%lu","command":"pushall"}})",
      static_cast<unsigned long>(sequenceId));
  if (written < 0 || static_cast<std::size_t>(written) >= outputCapacity)
    return 0;
  return static_cast<std::size_t>(written);
}

const char* bambuGenericTrayInfoIdx(const char* material) {
  if (material == nullptr || material[0] == '\0') return "";
  for (const auto& mapping : kGenericMaterialMappings) {
    if (materialStartsWithCaseInsensitive(material, mapping.material))
      return mapping.trayInfoIdx;
  }
  return "";
}

std::size_t bambuBuildAmsFilamentSetting(std::uint32_t sequenceId,
                                         std::uint8_t amsId,
                                         std::uint8_t trayId,
                                         const BambuTrayFilament& filament,
                                         char* output,
                                         std::size_t outputCapacity) {
  // tray_color must be an 8-digit RRGGBBAA hex string (alpha always FF per
  // docs/bambu-protocol.md); Spoolman's color_hex is 6-digit RRGGBB with no
  // alpha, so callers building BambuTrayFilament straight from a Spoolman
  // color get a 6-char string here -- append the alpha byte rather than
  // sending a malformed field.
  char colorWithAlpha[9]{};
  bambuNormalizeTrayColorHex(filament.trayColorHex, colorWithAlpha);

  char sequenceIdText[12]{};
  std::snprintf(sequenceIdText, sizeof(sequenceIdText), "%lu",
               static_cast<unsigned long>(sequenceId));

  JsonDocument document;
  JsonObject print = document["print"].to<JsonObject>();
  print["sequence_id"] = sequenceIdText;
  print["command"] = "ams_filament_setting";
  print["ams_id"] = amsId;
  print["tray_id"] = trayId;
  // "slot_id" duplicates tray_id (the slot's index within this AMS unit);
  // yanshay/spoolease's reverse-engineered driver sends both fields on this
  // command, see docs/bambu-protocol.md.
  print["slot_id"] = trayId;
  // Community-reverse-engineered payloads (see docs/bambu-protocol.md) list
  // tray_info_idx alongside tray_type/tray_color/nozzle_temp_*; omitting it
  // is a likely reason a real printer silently ignores a filament change
  // on an already-occupied slot even though the other fields are correct.
  print["tray_info_idx"] = bambuGenericTrayInfoIdx(filament.trayType);
  print["tray_type"] = filament.trayType;
  print["tray_color"] = colorWithAlpha;
  print["nozzle_temp_min"] = filament.nozzleTempMinC;
  print["nozzle_temp_max"] = filament.nozzleTempMaxC;

  const std::size_t written = serializeJson(document, output, outputCapacity);
  if (written == 0 || written >= outputCapacity) return 0;
  return written;
}

std::size_t bambuBuildExtrusionCaliSel(std::uint32_t sequenceId,
                                       std::uint8_t amsId,
                                       std::uint8_t trayId,
                                       const char* trayInfoIdx,
                                       const char* nozzleDiameter,
                                       std::int32_t caliIdx,
                                       char* output,
                                       std::size_t outputCapacity) {
  char sequenceIdText[12]{};
  std::snprintf(sequenceIdText, sizeof(sequenceIdText), "%lu",
               static_cast<unsigned long>(sequenceId));

  // Unlike ams_filament_setting, extrusion_cali_sel's "tray_id" is the
  // *global* tray index across all AMS units of this printer (amsId *
  // kSlotsPerAms + local slot index), not the local per-AMS index --
  // confirmed against yanshay/spoolease's reverse-engineered driver
  // (get_quad_for_set_filament_from_tray_id() passes "original_tray_id",
  // the un-split global index, as this command's tray_id, while slot_id
  // stays local). Doesn't affect a single-AMS printer (global == local for
  // ams_id 0), see docs/bambu-protocol.md. The external/manual slot
  // (amsId=kBambuExternalAmsId=255, trayId=kBambuExternalTrayId=254, see
  // PrinterState.h) is not part of any AMS unit's numbering at all -- the
  // multiplication below would wrap a std::uint8_t (255*4+254 = 1274 -> 250)
  // into a meaningless value. Nutzerbericht 2026-08-27: send the same fixed
  // sentinel (254) ams_filament_setting already uses for tray_id/slot_id in
  // this case instead of computing a global index.
  const std::uint8_t globalTrayId =
      amsId == models::kBambuExternalAmsId
          ? trayId
          : static_cast<std::uint8_t>(amsId * models::kSlotsPerAms + trayId);

  JsonDocument document;
  JsonObject print = document["print"].to<JsonObject>();
  print["command"] = "extrusion_cali_sel";
  print["cali_idx"] = caliIdx;
  print["filament_id"] = trayInfoIdx;
  print["nozzle_diameter"] = nozzleDiameter;
  print["ams_id"] = amsId;
  print["tray_id"] = globalTrayId;
  print["slot_id"] = trayId;
  print["sequence_id"] = sequenceIdText;

  const std::size_t written = serializeJson(document, output, outputCapacity);
  if (written == 0 || written >= outputCapacity) return 0;
  return written;
}

bool bambuApplyReport(const JsonDocument& document,
                      models::PrinterState& state) {
  if (!document["print"].is<JsonObjectConst>()) return false;
  const JsonObjectConst print = document["print"].as<JsonObjectConst>();

  if (print["ams"]["ams"].is<JsonArrayConst>()) {
    // Only a full "pushall" report carries ams.ams[] at all (a regular
    // periodic push_status typically omits it entirely, see
    // docs/bambu-protocol.md) -- so whenever this array *is* present, it is
    // a complete, authoritative snapshot of every currently attached AMS
    // unit. Track which ids appear in it so a unit that was present before
    // but is missing from this report (physically unplugged) can be
    // explicitly cleared below, rather than staying stuck "present"
    // forever (Robustheit/Diagnose, TASKS.md 10.5) -- every other field in
    // this function is merged/kept-at-last-known-value on purpose, but
    // presence has no such fallback: nothing else ever observes removal.
    std::array<bool, models::kMaximumAmsPerPrinter> seenAmsIds{};
    for (JsonObjectConst amsEntry :
        print["ams"]["ams"].as<JsonArrayConst>()) {
      std::uint8_t amsId = 0;
      if (!parseTrayIndex(amsEntry["id"], models::kMaximumAmsPerPrinter,
                          amsId))
        continue;
      seenAmsIds[amsId] = true;
      models::AmsState& amsState = state.amsUnits[amsId];
      amsState.amsId = amsId;
      amsState.present = true;
      amsState.connectionState = models::AmsConnectionState::Connected;

      if (!amsEntry["tray"].is<JsonArrayConst>()) continue;
      for (JsonObjectConst trayEntry : amsEntry["tray"].as<JsonArrayConst>()) {
        std::uint8_t trayId = 0;
        if (!parseTrayIndex(trayEntry["id"], models::kSlotsPerAms, trayId))
          continue;
        amsState.slots[trayId].trayId = trayId;
        applyTrayOccupancy(trayEntry, amsState.slots[trayId]);
      }
    }
    state.amsCount = 0;
    for (std::uint8_t amsId = 0; amsId < models::kMaximumAmsPerPrinter;
        ++amsId) {
      if (seenAmsIds[amsId]) {
        state.amsCount = static_cast<std::uint8_t>(amsId + 1);
        continue;
      }
      if (state.amsUnits[amsId].present) {
        state.amsUnits[amsId].present = false;
        state.amsUnits[amsId].connectionState =
            models::AmsConnectionState::Offline;
      }
    }
  }

  // "tray_now" reports which tray is currently loaded into the nozzle,
  // across all AMS units plus the external spool (Nutzerwunsch 2026-08-24,
  // see models::kActiveTrayNowExternal/kActiveTrayNowNone). Not every
  // report carries it -- keep the last known value otherwise, same as the
  // rest of this function.
  std::uint8_t trayNow = 0;
  if (parseTrayNow(print["ams"]["tray_now"], trayNow)) {
    state.activeTrayNow = trayNow;
  }

  if (print["vt_tray"].is<JsonObjectConst>()) {
    applyTrayOccupancy(print["vt_tray"].as<JsonObjectConst>(),
                       state.externalSlot);
  }

  if (print["nozzle_diameter"].is<const char*>()) {
    std::snprintf(state.nozzleDiameter, sizeof(state.nozzleDiameter), "%s",
                 print["nozzle_diameter"].as<const char*>());
  }
  return true;
}

}  // namespace services
}  // namespace filament_station
