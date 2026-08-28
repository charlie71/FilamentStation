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

/// @brief Whether a character is ignored when normalizing a material key,
///        so "PLA-CF"/"PLA CF"/"PLACF" all compare equal.
/// @param c Character to check.
/// @return true for '-', ' ', or '_'.
bool isMaterialKeySeparator(unsigned char c) {
  return c == '-' || c == ' ' || c == '_';
}

/// @brief Whether `material` matches one entry's canonical name or any of
///        its '|'-separated aliases (see models::BambuMaterialMappingEntry::aliases).
/// @param material Free-text material name to match.
/// @param entry Table entry to match against.
/// @return true if `material` normalizes to #entry's material or any alias.
bool materialMatchesEntry(const char* material,
                          const models::BambuMaterialMappingEntry& entry) {
  if (sameMaterialKey(material, entry.material)) return true;
  const char* cursor = entry.aliases;
  while (*cursor != '\0') {
    char token[models::kBambuMaterialFieldLength]{};
    std::size_t length = 0;
    while (*cursor != '\0' && *cursor != models::kBambuMaterialAliasSeparator &&
          length < sizeof(token) - 1U) {
      token[length++] = *cursor++;
    }
    token[length] = '\0';
    // Skip any remaining characters of an over-long token rather than
    // silently comparing a truncated prefix.
    while (*cursor != '\0' && *cursor != models::kBambuMaterialAliasSeparator)
      ++cursor;
    if (*cursor == models::kBambuMaterialAliasSeparator) ++cursor;
    if (length > 0 && sameMaterialKey(material, token)) return true;
  }
  return false;
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

/// @brief Parses a JSON value that may be a number or a numeric string into a std::uint16_t.
/// @param value JSON value to parse.
/// @param out Out parameter receiving the parsed value.
/// @return false if `value` is not a number/numeric string, or out of the uint16_t range.
bool parseUint16(JsonVariantConst value, std::uint16_t& out) {
  if (value.is<std::uint16_t>()) {
    out = value.as<std::uint16_t>();
    return true;
  }
  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 65535) return false;
    out = static_cast<std::uint16_t>(parsed);
    return true;
  }
  return false;
}

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
  // Not every report that carries occupancy fields also carries the
  // nozzle-temperature fields (e.g. the community-observed
  // "ams_filament_setting" command echo does, a regular tray entry inside
  // ams.ams[]/vt_tray may not) -- only overwrite when present and
  // parseable, keeping the last known value otherwise, same merge
  // behavior as the rest of bambuApplyReport().
  std::uint16_t nozzleTempMin = 0;
  if (parseUint16(trayJson["nozzle_temp_min"], nozzleTempMin))
    slot.nozzleTempMinC = nozzleTempMin;
  std::uint16_t nozzleTempMax = 0;
  if (parseUint16(trayJson["nozzle_temp_max"], nozzleTempMax))
    slot.nozzleTempMaxC = nozzleTempMax;
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

bool sameMaterialKey(const char* a, const char* b) {
  // Exact (post-normalization) matching rather than prefix matching -- a
  // prefix match would risk a more specific material (e.g. "PLA-CF") being
  // matched by a shorter, more general table entry ("PLA") depending on
  // table order; comparing full normalized keys sidesteps that entirely.
  for (;;) {
    while (*a != '\0' && isMaterialKeySeparator(static_cast<unsigned char>(*a)))
      ++a;
    while (*b != '\0' && isMaterialKeySeparator(static_cast<unsigned char>(*b)))
      ++b;
    if (*a == '\0' || *b == '\0') return *a == *b;
    if (std::toupper(static_cast<unsigned char>(*a)) !=
        std::toupper(static_cast<unsigned char>(*b)))
      return false;
    ++a;
    ++b;
  }
}

const models::BambuMaterialMappingEntry* resolveBambuMaterial(
    const models::BambuMaterialMappingTable& table, const char* material) {
  if (material == nullptr || material[0] == '\0') return nullptr;
  for (std::uint16_t index = 0; index < table.entryCount; ++index) {
    const models::BambuMaterialMappingEntry& entry = table.entries[index];
    if (materialMatchesEntry(material, entry)) return &entry;
  }
  return nullptr;
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
  // Resolved by the caller via resolveBambuMaterial() rather than looked up
  // here, so both this command and the following extrusion_cali_sel reuse
  // the exact same resolution instead of computing it twice.
  print["tray_info_idx"] = filament.trayInfoIdx;
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
