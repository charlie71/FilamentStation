#include <ArduinoJson.h>
#include <unity.h>

#include <cstdio>
#include <cstring>

#include "services/BambuProtocol.h"

using namespace filament_station;
using filament_station::models::PrinterSlotState;
using filament_station::models::PrinterState;

void setUp() {}
void tearDown() {}

namespace {

void testTopicsUseSerialNumber() {
  char report[64]{};
  char request[64]{};
  services::bambuReportTopic("01P00A123456789", report, sizeof(report));
  services::bambuRequestTopic("01P00A123456789", request, sizeof(request));
  TEST_ASSERT_EQUAL_STRING("device/01P00A123456789/report", report);
  TEST_ASSERT_EQUAL_STRING("device/01P00A123456789/request", request);
}

void testPushAllRequestPayload() {
  char payload[128]{};
  const std::size_t length =
      services::bambuBuildPushAllRequest(7, payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);
  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  TEST_ASSERT_EQUAL_STRING("pushall",
                           document["pushing"]["command"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING(
      "7", document["pushing"]["sequence_id"].as<const char*>());
}

void testPushAllRequestRejectsTooSmallBuffer() {
  char payload[4]{};
  TEST_ASSERT_EQUAL_UINT32(
      0, services::bambuBuildPushAllRequest(1, payload, sizeof(payload)));
}

void testAmsFilamentSettingPayload() {
  services::BambuTrayFilament filament{};
  std::snprintf(filament.trayInfoIdx, sizeof(filament.trayInfoIdx), "GFL99");
  std::snprintf(filament.trayType, sizeof(filament.trayType), "PLA");
  std::snprintf(filament.trayColorHex, sizeof(filament.trayColorHex),
               "FFFFFFFF");
  filament.nozzleTempMinC = 190;
  filament.nozzleTempMaxC = 240;

  char payload[256]{};
  const std::size_t length = services::bambuBuildAmsFilamentSetting(
      42, 1, 2, filament, payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);

  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  const JsonObjectConst print = document["print"];
  TEST_ASSERT_EQUAL_STRING("ams_filament_setting",
                           print["command"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("42",
                           print["sequence_id"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(1, print["ams_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(2, print["tray_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(2, print["slot_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_STRING("PLA", print["tray_type"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("FFFFFFFF", print["tray_color"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(190, print["nozzle_temp_min"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(240, print["nozzle_temp_max"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_STRING("GFL99", print["tray_info_idx"].as<const char*>());
  // setting_id is a concrete Bambu-Studio-preset concept this workflow
  // deliberately never resolves or sends.
  TEST_ASSERT_FALSE(print["setting_id"].is<const char*>());
}

void testAmsFilamentSettingAppendsAlphaToSixDigitColor() {
  // Spoolman's color_hex is 6-digit RRGGBB (no alpha); the wire protocol
  // requires 8-digit RRGGBBAA with alpha always FF.
  services::BambuTrayFilament filament{};
  std::snprintf(filament.trayType, sizeof(filament.trayType), "PETG");
  std::snprintf(filament.trayColorHex, sizeof(filament.trayColorHex),
               "00AE42");
  char payload[256]{};
  const std::size_t length = services::bambuBuildAmsFilamentSetting(
      1, 0, 0, filament, payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);
  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  TEST_ASSERT_EQUAL_STRING(
      "00AE42FF", document["print"]["tray_color"].as<const char*>());
}

void testNormalizeTrayColorHexAppendsAlpha() {
  char output[9]{};
  services::bambuNormalizeTrayColorHex("00AE42", output);
  TEST_ASSERT_EQUAL_STRING("00AE42FF", output);
}

void testNormalizeTrayColorHexPassesThroughEightDigits() {
  char output[9]{};
  services::bambuNormalizeTrayColorHex("00AE42FF", output);
  TEST_ASSERT_EQUAL_STRING("00AE42FF", output);
}

void testExtrusionCaliSelPayload() {
  char payload[256]{};
  const std::size_t length = services::bambuBuildExtrusionCaliSel(
      5, 0, 2, "GFL99", "0.4", -1, payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);

  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  const JsonObjectConst print = document["print"];
  TEST_ASSERT_EQUAL_STRING("extrusion_cali_sel",
                           print["command"].as<const char*>());
  TEST_ASSERT_EQUAL_INT32(-1, print["cali_idx"].as<std::int32_t>());
  TEST_ASSERT_EQUAL_STRING("GFL99", print["filament_id"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("0.4", print["nozzle_diameter"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(0, print["ams_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(2, print["tray_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(2, print["slot_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_STRING("5", print["sequence_id"].as<const char*>());
}

void testExtrusionCaliSelUsesGlobalTrayIdForSecondAms() {
  // Unlike ams_filament_setting, extrusion_cali_sel's wire "tray_id" is the
  // global index across all AMS units (amsId * kSlotsPerAms + local slot),
  // not the local per-AMS slot index -- confirmed against yanshay/spoolease.
  // AMS 1 (second unit), local slot 2 -> global tray_id 1*4+2=6.
  char payload[256]{};
  const std::size_t length = services::bambuBuildExtrusionCaliSel(
      1, 1, 2, "GFL99", "0.4", -1, payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);

  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  const JsonObjectConst print = document["print"];
  TEST_ASSERT_EQUAL_UINT32(1, print["ams_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(6, print["tray_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(2, print["slot_id"].as<std::uint32_t>());
}

void testExtrusionCaliSetPayload() {
  services::BambuCalibrationRequest request{};
  std::snprintf(request.filamentId, sizeof(request.filamentId), "GFL99");
  std::snprintf(request.settingId, sizeof(request.settingId), "FS000004D2");
  std::snprintf(request.name, sizeof(request.name), "FilamentStation #1234");
  std::snprintf(request.nozzleId, sizeof(request.nozzleId),
               "hardened_steel-0.4");
  request.kValue = 0.123F;

  char payload[384]{};
  const std::size_t length = services::bambuBuildExtrusionCaliSet(
      9, 0, 2, "0.4", request, payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);

  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  const JsonObjectConst print = document["print"];
  TEST_ASSERT_EQUAL_STRING("extrusion_cali_set",
                           print["command"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("0.4", print["nozzle_diameter"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("9", print["sequence_id"].as<const char*>());
  const JsonArrayConst filaments = print["filaments"];
  TEST_ASSERT_EQUAL_UINT32(1, filaments.size());
  const JsonObjectConst filament = filaments[0];
  TEST_ASSERT_EQUAL_UINT32(0, filament["ams_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_UINT32(0, filament["extruder_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_STRING("GFL99", filament["filament_id"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("0.123000", filament["k_value"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("0.000000", filament["n_coef"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("FilamentStation #1234",
                           filament["name"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("0.4",
                           filament["nozzle_diameter"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("hardened_steel-0.4",
                           filament["nozzle_id"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("FS000004D2",
                           filament["setting_id"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(2, filament["slot_id"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_INT32(-1, filament["tray_id"].as<std::int32_t>());
}

void testExtrusionCaliGetPayload() {
  char payload[256]{};
  const std::size_t length =
      services::bambuBuildExtrusionCaliGet(3, "0.4", payload, sizeof(payload));
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);

  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, payload, length));
  const JsonObjectConst print = document["print"];
  TEST_ASSERT_EQUAL_STRING("extrusion_cali_get",
                           print["command"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("", print["filament_id"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("0.4", print["nozzle_diameter"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("3", print["sequence_id"].as<const char*>());
}

void testFindCalibrationBySettingIdMatches() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {
      "command": "extrusion_cali_get",
      "filaments": [
        {"setting_id":"OTHER","cali_idx":1},
        {"setting_id":"FS000004D2","cali_idx":7}
      ]
    }
  })");
  const auto result =
      services::bambuFindCalibrationBySettingId(document, "FS000004D2");
  TEST_ASSERT_TRUE(result.found);
  TEST_ASSERT_EQUAL_INT32(7, result.caliIdx);
}

void testFindCalibrationBySettingIdNoMatch() {
  JsonDocument withoutMatch;
  deserializeJson(withoutMatch, R"({
    "print": {"filaments": [{"setting_id":"OTHER","cali_idx":1}]}
  })");
  TEST_ASSERT_FALSE(
      services::bambuFindCalibrationBySettingId(withoutMatch, "FS000004D2")
          .found);

  JsonDocument emptyList;
  deserializeJson(emptyList, R"({"print": {"filaments": []}})");
  TEST_ASSERT_FALSE(
      services::bambuFindCalibrationBySettingId(emptyList, "FS000004D2")
          .found);

  JsonDocument noFilaments;
  deserializeJson(noFilaments, R"({"print": {"command":"extrusion_cali_get"}})");
  TEST_ASSERT_FALSE(
      services::bambuFindCalibrationBySettingId(noFilaments, "FS000004D2")
          .found);

  JsonDocument noPrint;
  deserializeJson(noPrint, R"({"system":{}})");
  TEST_ASSERT_FALSE(
      services::bambuFindCalibrationBySettingId(noPrint, "FS000004D2").found);
}

void testApplyReportRejectsPayloadWithoutPrintObject() {
  JsonDocument document;
  deserializeJson(document, R"({"system":{"command":"other"}})");
  PrinterState state{};
  TEST_ASSERT_FALSE(services::bambuApplyReport(document, state));
}

void testApplyReportParsesAmsTraysWithStringIds() {
  JsonDocument document;
  const auto error = deserializeJson(document, R"({
    "print": {
      "ams": {
        "ams": [
          {"id":"0","tray":[
            {"id":"0","tray_type":"PLA","tray_color":"FFFFFFFF"},
            {"id":"1","tray_type":"","tray_color":""},
            {"id":"2","tray_type":"PETG","tray_color":"000000FF"},
            {"id":"3","tray_type":"","tray_color":""}
          ]}
        ]
      }
    }
  })");
  TEST_ASSERT_FALSE(error);

  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_UINT8(1, state.amsCount);
  TEST_ASSERT_TRUE(state.amsUnits[0].present);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(models::AmsConnectionState::Connected),
      static_cast<int>(state.amsUnits[0].connectionState));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Ready),
                        static_cast<int>(state.amsUnits[0].slots[0].state));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Empty),
                        static_cast<int>(state.amsUnits[0].slots[1].state));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Ready),
                        static_cast<int>(state.amsUnits[0].slots[2].state));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Empty),
                        static_cast<int>(state.amsUnits[0].slots[3].state));
  TEST_ASSERT_EQUAL_STRING("PLA", state.amsUnits[0].slots[0].material);
  TEST_ASSERT_EQUAL_STRING("FFFFFFFF", state.amsUnits[0].slots[0].colorHex);
  TEST_ASSERT_EQUAL_STRING("", state.amsUnits[0].slots[1].material);
  TEST_ASSERT_EQUAL_STRING("PETG", state.amsUnits[0].slots[2].material);
  TEST_ASSERT_EQUAL_STRING("000000FF", state.amsUnits[0].slots[2].colorHex);
}

void testApplyReportParsesAmsWithNumericIds() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {
      "ams": {"ams": [{"id":1,"tray":[{"id":0,"tray_type":"ABS"}]}]}
    }
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_TRUE(state.amsUnits[1].present);
  TEST_ASSERT_EQUAL_UINT8(2, state.amsCount);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Ready),
                        static_cast<int>(state.amsUnits[1].slots[0].state));
}

void testApplyReportIgnoresOutOfRangeIds() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {
      "ams": {"ams": [{"id":"9","tray":[{"id":"0","tray_type":"PLA"}]}]}
    }
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_UINT8(0, state.amsCount);
  for (const auto& ams : state.amsUnits) TEST_ASSERT_FALSE(ams.present);
}

void testApplyReportClearsAmsRemovedFromFullReport() {
  JsonDocument first;
  deserializeJson(first, R"({
    "print": {
      "ams": {"ams": [
        {"id":"0","tray":[{"id":"0","tray_type":"PLA"}]},
        {"id":"1","tray":[{"id":"0","tray_type":"PETG"}]}
      ]}
    }
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(first, state));
  TEST_ASSERT_TRUE(state.amsUnits[0].present);
  TEST_ASSERT_TRUE(state.amsUnits[1].present);
  TEST_ASSERT_EQUAL_UINT8(2, state.amsCount);

  // AMS 1 physically unplugged: the next full report's ams[] no longer
  // lists it -- since this array only ever appears on a full "pushall"
  // (see docs/bambu-protocol.md), its absence here is authoritative, not
  // a partial update that should leave AMS 1 untouched.
  JsonDocument second;
  deserializeJson(second, R"({
    "print": {
      "ams": {"ams": [
        {"id":"0","tray":[{"id":"0","tray_type":"PLA"}]}
      ]}
    }
  })");
  TEST_ASSERT_TRUE(services::bambuApplyReport(second, state));
  TEST_ASSERT_TRUE(state.amsUnits[0].present);
  TEST_ASSERT_FALSE(state.amsUnits[1].present);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(models::AmsConnectionState::Offline),
      static_cast<int>(state.amsUnits[1].connectionState));
  TEST_ASSERT_EQUAL_UINT8(1, state.amsCount);
}

void testApplyReportPartialUpdateKeepsAmsPresence() {
  JsonDocument first;
  deserializeJson(first, R"({
    "print": {
      "ams": {"ams": [{"id":"0","tray":[{"id":"0","tray_type":"PLA"}]}]}
    }
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(first, state));
  TEST_ASSERT_TRUE(state.amsUnits[0].present);

  // A regular periodic push_status typically omits ams.ams[] entirely (see
  // docs/bambu-protocol.md) -- its absence here must NOT be read as "no AMS
  // attached", unlike a full report that includes the (possibly smaller)
  // array explicitly.
  JsonDocument partial;
  deserializeJson(partial, R"({"print": {"bed_temper": 23.5}})");
  TEST_ASSERT_TRUE(services::bambuApplyReport(partial, state));
  TEST_ASSERT_TRUE(state.amsUnits[0].present);
  TEST_ASSERT_EQUAL_UINT8(1, state.amsCount);
}

void testApplyReportParsesExternalTray() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {"vt_tray": {"tray_type":"PETG","tray_color":"00FF00FF"}}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PrinterSlotState::Ready),
                        static_cast<int>(state.externalSlot.state));
  TEST_ASSERT_EQUAL_STRING("PETG", state.externalSlot.material);
  TEST_ASSERT_EQUAL_STRING("00FF00FF", state.externalSlot.colorHex);
}

void testApplyReportParsesNozzleTempAsNumber() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {"vt_tray": {"tray_type":"PLA","nozzle_temp_min":190,"nozzle_temp_max":240}}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_UINT16(190, state.externalSlot.nozzleTempMinC);
  TEST_ASSERT_EQUAL_UINT16(240, state.externalSlot.nozzleTempMaxC);
}

void testApplyReportParsesNozzleTempAsString() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {"vt_tray": {"tray_type":"PLA","nozzle_temp_min":"190","nozzle_temp_max":"240"}}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_UINT16(190, state.externalSlot.nozzleTempMinC);
  TEST_ASSERT_EQUAL_UINT16(240, state.externalSlot.nozzleTempMaxC);
}

void testApplyReportKeepsNozzleTempWhenAbsentFromLaterReport() {
  JsonDocument first;
  deserializeJson(first, R"({
    "print": {"vt_tray": {"tray_type":"PLA","nozzle_temp_min":190,"nozzle_temp_max":240}}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(first, state));

  // A later report for the same tray without the temperature fields must
  // not reset them to 0 -- same merge behavior as the rest of
  // bambuApplyReport().
  JsonDocument second;
  deserializeJson(second, R"({
    "print": {"vt_tray": {"tray_type":"PLA","tray_color":"FFFFFFFF"}}
  })");
  TEST_ASSERT_TRUE(services::bambuApplyReport(second, state));
  TEST_ASSERT_EQUAL_UINT16(190, state.externalSlot.nozzleTempMinC);
  TEST_ASSERT_EQUAL_UINT16(240, state.externalSlot.nozzleTempMaxC);
}

void testApplyReportParsesNozzleDiameter() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {"nozzle_diameter": "0.4"}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_STRING("0.4", state.nozzleDiameter);
}

void testApplyReportParsesNozzleType() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {"nozzle_type": "hardened_steel"}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_STRING("hardened_steel", state.nozzleType);

  // A later report without the field must not reset it (same merge
  // behavior as nozzleDiameter/the rest of bambuApplyReport()).
  JsonDocument partial;
  deserializeJson(partial, R"({"print": {"nozzle_diameter": "0.4"}})");
  TEST_ASSERT_TRUE(services::bambuApplyReport(partial, state));
  TEST_ASSERT_EQUAL_STRING("hardened_steel", state.nozzleType);
}

void testApplyReportParsesTrayNow() {
  // Global AMS slot (string form, as the printer sends it): AMS 1 slot 2 ->
  // amsId 1 * kSlotsPerAms 4 + trayId 2 = 6.
  JsonDocument amsSlot;
  deserializeJson(amsSlot, R"({"print": {"ams": {"tray_now": "6"}}})");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(amsSlot, state));
  TEST_ASSERT_EQUAL_UINT8(6, state.activeTrayNow);

  // External/vt_tray sentinel (numeric form).
  JsonDocument external;
  deserializeJson(external, R"({"print": {"ams": {"tray_now": 254}}})");
  PrinterState externalState{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(external, externalState));
  TEST_ASSERT_EQUAL_UINT8(models::kActiveTrayNowExternal,
                          externalState.activeTrayNow);
  TEST_ASSERT_EQUAL_UINT8(254, externalState.activeTrayNow);

  // No tray active.
  JsonDocument none;
  deserializeJson(none, R"({"print": {"ams": {"tray_now": "255"}}})");
  PrinterState noneState{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(none, noneState));
  TEST_ASSERT_EQUAL_UINT8(models::kActiveTrayNowNone, noneState.activeTrayNow);

  // Field absent from this report: keeps the last known value rather than
  // resetting (same merge behavior as the rest of bambuApplyReport()).
  JsonDocument partial;
  deserializeJson(partial, R"({"print": {"nozzle_diameter": "0.4"}})");
  PrinterState carried{};
  carried.activeTrayNow = 6;
  TEST_ASSERT_TRUE(services::bambuApplyReport(partial, carried));
  TEST_ASSERT_EQUAL_UINT8(6, carried.activeTrayNow);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testTopicsUseSerialNumber);
  RUN_TEST(testPushAllRequestPayload);
  RUN_TEST(testPushAllRequestRejectsTooSmallBuffer);
  RUN_TEST(testAmsFilamentSettingPayload);
  RUN_TEST(testAmsFilamentSettingAppendsAlphaToSixDigitColor);
  RUN_TEST(testNormalizeTrayColorHexAppendsAlpha);
  RUN_TEST(testNormalizeTrayColorHexPassesThroughEightDigits);
  RUN_TEST(testExtrusionCaliSelPayload);
  RUN_TEST(testExtrusionCaliSelUsesGlobalTrayIdForSecondAms);
  RUN_TEST(testExtrusionCaliSetPayload);
  RUN_TEST(testExtrusionCaliGetPayload);
  RUN_TEST(testFindCalibrationBySettingIdMatches);
  RUN_TEST(testFindCalibrationBySettingIdNoMatch);
  RUN_TEST(testApplyReportRejectsPayloadWithoutPrintObject);
  RUN_TEST(testApplyReportParsesAmsTraysWithStringIds);
  RUN_TEST(testApplyReportParsesAmsWithNumericIds);
  RUN_TEST(testApplyReportIgnoresOutOfRangeIds);
  RUN_TEST(testApplyReportClearsAmsRemovedFromFullReport);
  RUN_TEST(testApplyReportPartialUpdateKeepsAmsPresence);
  RUN_TEST(testApplyReportParsesExternalTray);
  RUN_TEST(testApplyReportParsesNozzleTempAsNumber);
  RUN_TEST(testApplyReportParsesNozzleTempAsString);
  RUN_TEST(testApplyReportKeepsNozzleTempWhenAbsentFromLaterReport);
  RUN_TEST(testApplyReportParsesNozzleDiameter);
  RUN_TEST(testApplyReportParsesNozzleType);
  RUN_TEST(testApplyReportParsesTrayNow);
  return UNITY_END();
}
