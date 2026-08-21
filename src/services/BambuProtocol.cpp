#include "services/BambuProtocol.h"

#include <cstdio>
#include <cstdlib>

namespace filament_station {
namespace services {
namespace {

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

void applyTrayOccupancy(JsonObjectConst trayJson,
                        models::PrinterSlotStateData& slot) {
  const char* trayType = trayJson["tray_type"] | "";
  slot.state = trayType[0] != '\0' ? models::PrinterSlotState::Ready
                                   : models::PrinterSlotState::Empty;
  std::snprintf(slot.material, sizeof(slot.material), "%s", trayType);
  const char* trayColor = trayJson["tray_color"] | "";
  std::snprintf(slot.colorHex, sizeof(slot.colorHex), "%s", trayColor);
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

std::size_t bambuBuildPushAllRequest(char* output,
                                     std::size_t outputCapacity) {
  const int written = std::snprintf(
      output, outputCapacity,
      R"({"pushing":{"sequence_id":"0","command":"pushall"}})");
  if (written < 0 || static_cast<std::size_t>(written) >= outputCapacity)
    return 0;
  return static_cast<std::size_t>(written);
}

std::size_t bambuBuildAmsFilamentSetting(std::uint8_t amsId,
                                         std::uint8_t trayId,
                                         const BambuTrayFilament& filament,
                                         char* output,
                                         std::size_t outputCapacity) {
  JsonDocument document;
  JsonObject print = document["print"].to<JsonObject>();
  print["sequence_id"] = "0";
  print["command"] = "ams_filament_setting";
  print["ams_id"] = amsId;
  print["tray_id"] = trayId;
  print["tray_type"] = filament.trayType;
  print["tray_color"] = filament.trayColorHex;
  print["nozzle_temp_min"] = filament.nozzleTempMinC;
  print["nozzle_temp_max"] = filament.nozzleTempMaxC;

  const std::size_t written = serializeJson(document, output, outputCapacity);
  if (written == 0 || written >= outputCapacity) return 0;
  return written;
}

bool bambuApplyReport(const JsonDocument& document,
                      models::PrinterState& state) {
  if (!document["print"].is<JsonObjectConst>()) return false;
  const JsonObjectConst print = document["print"].as<JsonObjectConst>();

  if (print["ams"]["ams"].is<JsonArrayConst>()) {
    for (JsonObjectConst amsEntry :
        print["ams"]["ams"].as<JsonArrayConst>()) {
      std::uint8_t amsId = 0;
      if (!parseTrayIndex(amsEntry["id"], models::kMaximumAmsPerPrinter,
                          amsId))
        continue;
      models::AmsState& amsState = state.amsUnits[amsId];
      amsState.amsId = amsId;
      amsState.present = true;
      amsState.connectionState = models::AmsConnectionState::Connected;
      if (state.amsCount <= amsId)
        state.amsCount = static_cast<std::uint8_t>(amsId + 1);

      if (!amsEntry["tray"].is<JsonArrayConst>()) continue;
      for (JsonObjectConst trayEntry : amsEntry["tray"].as<JsonArrayConst>()) {
        std::uint8_t trayId = 0;
        if (!parseTrayIndex(trayEntry["id"], models::kSlotsPerAms, trayId))
          continue;
        amsState.slots[trayId].trayId = trayId;
        applyTrayOccupancy(trayEntry, amsState.slots[trayId]);
      }
    }
  }

  if (print["vt_tray"].is<JsonObjectConst>()) {
    applyTrayOccupancy(print["vt_tray"].as<JsonObjectConst>(),
                       state.externalSlot);
  }
  return true;
}

}  // namespace services
}  // namespace filament_station
