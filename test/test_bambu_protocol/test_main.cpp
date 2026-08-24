#include <ArduinoJson.h>
#include <unity.h>

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

void testGenericTrayInfoIdxMapping() {
  // Composite ("-CF") variants must win over their plain base material.
  TEST_ASSERT_EQUAL_STRING("GFL98",
                           services::bambuGenericTrayInfoIdx("PLA-CF"));
  TEST_ASSERT_EQUAL_STRING("GFN98",
                           services::bambuGenericTrayInfoIdx("PA-CF"));
  TEST_ASSERT_EQUAL_STRING("GFL99", services::bambuGenericTrayInfoIdx("PLA"));
  // Free-text Spoolman material strings (e.g. "PLA Basic") still match on
  // the material family prefix.
  TEST_ASSERT_EQUAL_STRING("GFL99",
                           services::bambuGenericTrayInfoIdx("PLA Basic"));
  TEST_ASSERT_EQUAL_STRING("GFG99", services::bambuGenericTrayInfoIdx("PETG"));
  TEST_ASSERT_EQUAL_STRING("GFB98", services::bambuGenericTrayInfoIdx("ASA"));
  TEST_ASSERT_EQUAL_STRING("GFB99", services::bambuGenericTrayInfoIdx("ABS"));
  TEST_ASSERT_EQUAL_STRING("GFU99", services::bambuGenericTrayInfoIdx("TPU"));
  TEST_ASSERT_EQUAL_STRING("GFS99", services::bambuGenericTrayInfoIdx("PVA"));
  TEST_ASSERT_EQUAL_STRING("GFC99", services::bambuGenericTrayInfoIdx("PC"));
  TEST_ASSERT_EQUAL_STRING("GFN99", services::bambuGenericTrayInfoIdx("PA"));
  // Unknown/empty material: no invented id.
  TEST_ASSERT_EQUAL_STRING("", services::bambuGenericTrayInfoIdx("Wood"));
  TEST_ASSERT_EQUAL_STRING("", services::bambuGenericTrayInfoIdx(""));
  TEST_ASSERT_EQUAL_STRING("", services::bambuGenericTrayInfoIdx(nullptr));
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

void testApplyReportParsesNozzleDiameter() {
  JsonDocument document;
  deserializeJson(document, R"({
    "print": {"nozzle_diameter": "0.4"}
  })");
  PrinterState state{};
  TEST_ASSERT_TRUE(services::bambuApplyReport(document, state));
  TEST_ASSERT_EQUAL_STRING("0.4", state.nozzleDiameter);
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
  RUN_TEST(testGenericTrayInfoIdxMapping);
  RUN_TEST(testApplyReportRejectsPayloadWithoutPrintObject);
  RUN_TEST(testApplyReportParsesAmsTraysWithStringIds);
  RUN_TEST(testApplyReportParsesAmsWithNumericIds);
  RUN_TEST(testApplyReportIgnoresOutOfRangeIds);
  RUN_TEST(testApplyReportParsesExternalTray);
  RUN_TEST(testApplyReportParsesNozzleDiameter);
  RUN_TEST(testApplyReportParsesTrayNow);
  return UNITY_END();
}
