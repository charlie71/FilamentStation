#include <ArduinoJson.h>
#include <unity.h>

#include <fstream>
#include <sstream>

#include "services/BambuMaterialCatalog.h"

using namespace filament_station;

void setUp() {}
void tearDown() {}

namespace {

// ---------------------------------------------------------------------
// Parser/validator tests (schema v2)
// ---------------------------------------------------------------------

void testValidSchemaV2WithMultipleRules() {
  JsonDocument document;
  const auto parseError = deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {
        "id": "generic-pla",
        "priority": 10,
        "match": {"material_exact": ["PLA", "PLA+"]},
        "result": {"status": "mapped", "tray_info_idx": "GFL99",
                   "tray_type": "PLA", "nozzle_temp_min": 190,
                   "nozzle_temp_max": 240}
      },
      {
        "id": "generic-pla-silk",
        "priority": 100,
        "match": {"material_exact": ["PLA", "PLA+"],
                  "name_contains_any": ["Silk"]},
        "result": {"status": "mapped", "tray_info_idx": "GFL96",
                   "tray_type": "PLA", "nozzle_temp_min": 190,
                   "nozzle_temp_max": 240}
      },
      {
        "id": "unsupported-wood",
        "priority": 10,
        "match": {"material_exact": ["Wood"]},
        "result": {"status": "unsupported", "reason": "Base polymer is unknown"}
      }
    ]
  })");
  TEST_ASSERT_FALSE(parseError);

  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  TEST_ASSERT_EQUAL_UINT16(3, table.ruleCount);
  TEST_ASSERT_EQUAL_UINT32(2, table.schemaVersion);
  TEST_ASSERT_EQUAL_STRING("generic-pla", table.rules[0].id);
  TEST_ASSERT_EQUAL_INT32(10, table.rules[0].priority);
  TEST_ASSERT_EQUAL_STRING("PLA|PLA+", table.rules[0].match.materialExact);
  TEST_ASSERT_EQUAL_STRING("GFL99", table.rules[0].result.trayInfoIdx);
  TEST_ASSERT_EQUAL_STRING("Silk", table.rules[1].match.nameContainsAny);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(models::BambuMaterialRuleResultStatus::Unsupported),
      static_cast<int>(table.rules[2].result.status));
  TEST_ASSERT_EQUAL_STRING("Base polymer is unknown", table.rules[2].result.reason);
}

void testInvalidJsonIsRejected() {
  JsonDocument arrayRoot;
  deserializeJson(arrayRoot, R"(["not", "an", "object"])");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(arrayRoot, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::InvalidJson),
                        static_cast<int>(result.error));
}

void testMissingSchemaVersionIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({"rules": []})");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingSchemaVersion),
      static_cast<int>(result.error));
}

void testWrongSchemaVersionIsRejected() {
  // Only schema v2 is accepted -- the old flat schema v1 must be rejected,
  // not silently migrated at load time (Nutzerwunsch 2026-08-30, decided
  // against maintaining two parallel runtime resolvers).
  JsonDocument document;
  deserializeJson(document, R"({"schema_version": 1, "materials": []})");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::UnsupportedSchemaVersion),
      static_cast<int>(result.error));
}

void testMissingRulesArrayIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({"schema_version": 2})");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingRulesArray),
      static_cast<int>(result.error));
}

void testEmptyRulesArrayIsValid() {
  JsonDocument document;
  deserializeJson(document, R"({"schema_version": 2, "rules": []})");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  TEST_ASSERT_EQUAL_UINT16(0, table.ruleCount);
}

void testMissingRuleIdIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"priority": 10, "match": {"material_exact": ["PLA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99",
                  "tray_type": "PLA", "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingRuleId),
      static_cast<int>(result.error));
}

void testDuplicateRuleIdIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10, "match": {"material_exact": ["PLA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99",
                  "tray_type": "PLA", "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "generic-pla", "priority": 20, "match": {"material_exact": ["PLA+"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99",
                  "tray_type": "PLA", "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::DuplicateRuleId),
      static_cast<int>(result.error));
}

void testMissingPriorityIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "match": {"material_exact": ["PLA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99",
                  "tray_type": "PLA", "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingPriority),
      static_cast<int>(result.error));
}

void testEmptyMatchIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10, "match": {},
       "result": {"status": "mapped", "tray_info_idx": "GFL99",
                  "tray_type": "PLA", "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::EmptyMatch),
      static_cast<int>(result.error));
}

void testInvalidStatusIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10, "match": {"material_exact": ["PLA"]},
       "result": {"status": "sort-of-mapped"}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::InvalidStatus),
      static_cast<int>(result.error));
}

void testMappedWithoutTrayInfoIdxIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10, "match": {"material_exact": ["PLA"]},
       "result": {"status": "mapped", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingRequiredField),
      static_cast<int>(result.error));
}

void testInvalidTemperatureRangeIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10, "match": {"material_exact": ["PLA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99", "tray_type": "PLA",
                  "nozzle_temp_min": 240, "nozzle_temp_max": 190}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::InvalidTemperatureRange),
      static_cast<int>(result.error));
}

void testUnsupportedWithoutReasonIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "unsupported-wood", "priority": 10, "match": {"material_exact": ["Wood"]},
       "result": {"status": "unsupported"}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingReason),
      static_cast<int>(result.error));
}

void testWrongDataTypeInMatchArrayIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10, "match": {"material_exact": [190]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::InvalidMatchFieldType),
      static_cast<int>(result.error));
}

void testErrorNamesAreStable() {
  TEST_ASSERT_EQUAL_STRING(
      "ok", services::bambuMaterialCatalogErrorName(
                services::BambuMaterialCatalogError::Ok));
  TEST_ASSERT_EQUAL_STRING(
      "duplicate_rule_id",
      services::bambuMaterialCatalogErrorName(
          services::BambuMaterialCatalogError::DuplicateRuleId));
  TEST_ASSERT_EQUAL_STRING(
      "unsupported_schema_version",
      services::bambuMaterialCatalogErrorName(
          services::BambuMaterialCatalogError::UnsupportedSchemaVersion));
}

// ---------------------------------------------------------------------
// Resolver tests
// ---------------------------------------------------------------------

/// @brief Representative subset of data/bambu-materials/bambu_materials.json
///        covering every case the resolver tests below exercise (generic
///        fallbacks, Bambu-specific profiles gated on name/manufacturer,
///        support materials outranking generics, and deliberately
///        unsupported materials).
models::BambuMaterialRuleTable buildTestRuleTable() {
  JsonDocument document;
  const auto parseError = deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "generic-pla", "priority": 10,
       "match": {"material_exact": ["PLA", "PLA+"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL99", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "generic-pla-silk", "priority": 100,
       "match": {"material_exact": ["PLA", "PLA+"], "name_contains_any": ["Silk"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL96", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "generic-pla-high-speed", "priority": 100,
       "match": {"material_exact": ["PLA", "PLA+"],
                 "name_contains_any": ["High Speed", "High-Speed"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL95", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "bambu-pla-basic", "priority": 200,
       "match": {"material_exact": ["PLA"], "manufacturer_exact": ["Bambu Lab"],
                 "name_contains_any": ["PLA Basic"]},
       "result": {"status": "mapped", "tray_info_idx": "GFA00", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "generic-petg", "priority": 10,
       "match": {"material_exact": ["PETG"]},
       "result": {"status": "mapped", "tray_info_idx": "GFG99", "tray_type": "PETG",
                  "nozzle_temp_min": 220, "nozzle_temp_max": 260}},
      {"id": "generic-petg-hf", "priority": 100,
       "match": {"material_exact": ["PETG"],
                 "name_contains_any": ["PETG HF", "High Flow"]},
       "result": {"status": "mapped", "tray_info_idx": "GFG96", "tray_type": "PETG",
                  "nozzle_temp_min": 220, "nozzle_temp_max": 270}},
      {"id": "generic-abs", "priority": 10,
       "match": {"material_exact": ["ABS", "ABS+", "ABS-T"]},
       "result": {"status": "mapped", "tray_info_idx": "GFB99", "tray_type": "ABS",
                  "nozzle_temp_min": 240, "nozzle_temp_max": 280}},
      {"id": "generic-pla-cf", "priority": 10,
       "match": {"material_exact": ["PLA-CF", "PLA CF", "PLACF"]},
       "result": {"status": "mapped", "tray_info_idx": "GFL98", "tray_type": "PLA-CF",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "generic-petg-cf", "priority": 10,
       "match": {"material_exact": ["PETG-CF"]},
       "result": {"status": "mapped", "tray_info_idx": "GFG98", "tray_type": "PETG-CF",
                  "nozzle_temp_min": 220, "nozzle_temp_max": 260}},
      {"id": "generic-pa-cf", "priority": 10,
       "match": {"material_exact": ["PA-CF"]},
       "result": {"status": "mapped", "tray_info_idx": "GFN98", "tray_type": "PA-CF",
                  "nozzle_temp_min": 260, "nozzle_temp_max": 300}},
      {"id": "generic-pva", "priority": 10,
       "match": {"material_exact": ["PVA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFS99", "tray_type": "PVA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "bambu-pva", "priority": 100,
       "match": {"material_exact": ["Bambu PVA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFS04", "tray_type": "PVA",
                  "nozzle_temp_min": 220, "nozzle_temp_max": 250}},
      {"id": "support-for-pla", "priority": 100,
       "match": {"material_exact": ["Support For PLA", "Support for PLA", "Support PLA"]},
       "result": {"status": "mapped", "tray_info_idx": "GFS02", "tray_type": "PLA-S",
                  "nozzle_temp_min": 220, "nozzle_temp_max": 230}},
      {"id": "support-for-pla-petg", "priority": 100,
       "match": {"material_exact": ["Support For PLA/PETG", "Support for PLA/PETG", "Support PLA PETG"]},
       "result": {"status": "mapped", "tray_info_idx": "GFS05", "tray_type": "PLA-S",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 220}},
      {"id": "support-for-abs", "priority": 100,
       "match": {"material_exact": ["Support For ABS", "Support for ABS", "Support ABS"]},
       "result": {"status": "mapped", "tray_info_idx": "GFS06", "tray_type": "ABS-S",
                  "nozzle_temp_min": 240, "nozzle_temp_max": 270}},
      {"id": "support-for-pa-pet", "priority": 100,
       "match": {"material_exact": ["Support For PA/PET", "Support for PA/PET", "Support PA PET"]},
       "result": {"status": "mapped", "tray_info_idx": "GFS03", "tray_type": "PA-S",
                  "nozzle_temp_min": 280, "nozzle_temp_max": 300}},
      {"id": "unsupported-carbon-fiber", "priority": 10,
       "match": {"material_exact": ["Carbon Fiber"]},
       "result": {"status": "unsupported", "reason": "Base polymer is unknown"}},
      {"id": "unsupported-wood", "priority": 10,
       "match": {"material_exact": ["Wood"]},
       "result": {"status": "unsupported", "reason": "Base polymer is unknown"}},
      {"id": "unsupported-pc-abs", "priority": 10,
       "match": {"material_exact": ["PC/ABS"]},
       "result": {"status": "unsupported", "reason": "No verified Bambu profile for this blend"}},
      {"id": "unsupported-pvb", "priority": 10,
       "match": {"material_exact": ["PVB"]},
       "result": {"status": "unsupported", "reason": "No verified Bambu profile for this material"}}
    ]
  })");
  TEST_ASSERT_FALSE(parseError);
  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  return table;
}

void testResolveGenericPla() {
  const auto table = buildTestRuleTable();
  const auto result = services::resolveBambuMaterialRule(table, {"PLA", "", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("generic-pla", result.rule->id);
  TEST_ASSERT_EQUAL_STRING("GFL99", result.rule->result.trayInfoIdx);
}

void testResolveGenericPlaPlus() {
  // Regression: PLA+ must fall onto the generic PLA rule (acceptance
  // criterion 6), not be left unresolved.
  const auto table = buildTestRuleTable();
  const auto result = services::resolveBambuMaterialRule(table, {"PLA+", "", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("GFL99", result.rule->result.trayInfoIdx);
}

void testResolveGenericAbsVariants() {
  // Regression: ABS+/ABS-T must fall onto the generic ABS rule
  // (acceptance criterion 7).
  const auto table = buildTestRuleTable();
  for (const char* material : {"ABS", "ABS+", "ABS-T"}) {
    const auto result =
        services::resolveBambuMaterialRule(table, {material, "", ""});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("GFB99", result.rule->result.trayInfoIdx);
  }
}

void testResolvePlaSilk() {
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"PLA", "Super Silk PLA", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("generic-pla-silk", result.rule->id);
  TEST_ASSERT_EQUAL_STRING("GFL96", result.rule->result.trayInfoIdx);
}

void testResolvePlaHighSpeed() {
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"PLA", "PLA High Speed", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("GFL95", result.rule->result.trayInfoIdx);
}

void testResolvePlaPlusHighSpeed() {
  const auto table = buildTestRuleTable();
  const auto result = services::resolveBambuMaterialRule(
      table, {"PLA+", "High Speed PLA+", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("GFL95", result.rule->result.trayInfoIdx);
}

void testResolvePetgHf() {
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"PETG", "PETG HF", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("GFG96", result.rule->result.trayInfoIdx);
}

void testResolveNormalPetg() {
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"PETG", "PETG Basic", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("GFG99", result.rule->result.trayInfoIdx);
}

void testResolveBambuPlaBasic() {
  const auto table = buildTestRuleTable();
  const auto result = services::resolveBambuMaterialRule(
      table, {"PLA", "PLA Basic", "Bambu Lab"});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("bambu-pla-basic", result.rule->id);
  TEST_ASSERT_EQUAL_STRING("GFA00", result.rule->result.trayInfoIdx);
}

void testResolveNonBambuPlaBasicFallsBackToGeneric() {
  // A third-party product also named "PLA Basic" must never be mistaken
  // for the Bambu-manufactured one (acceptance criterion 11).
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"PLA", "PLA Basic", "Other"});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("generic-pla", result.rule->id);
  TEST_ASSERT_EQUAL_STRING("GFL99", result.rule->result.trayInfoIdx);
}

void testResolveSupportMaterialsOutrankGeneric() {
  const auto table = buildTestRuleTable();

  auto expect = [&](const char* material, const char* name,
                    const char* trayInfoIdx) {
    const auto result =
        services::resolveBambuMaterialRule(table, {material, name, ""});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING(trayInfoIdx, result.rule->result.trayInfoIdx);
  };
  // "material" here plays the role Spoolman actually stores support
  // filaments under -- the support product name itself, not the base
  // polymer -- matching the material_exact rules above (docs/bambu-
  // protocol.md's existing "Support For ..." convention).
  expect("Support For PLA", "", "GFS02");
  expect("Support For PLA/PETG", "", "GFS05");
  expect("Support For ABS", "", "GFS06");
  expect("Support For PA/PET", "", "GFS03");
  expect("Bambu PVA", "", "GFS04");
}

void testResolveSupportForPlaDoesNotFallBackToGenericPla() {
  // The exact scenario from the task description (section 18): a spool
  // whose base material is PLA but whose actual product is "Support For
  // PLA" must resolve via the support rule, not the generic PLA fallback.
  const auto table = buildTestRuleTable();
  const auto result = services::resolveBambuMaterialRule(
      table, {"PLA", "Support For PLA", ""});
  // The support material_exact list only matches the *material* field in
  // this fixture (mirroring Spoolman storing support filaments with their
  // product name as the material itself); with material="PLA" and
  // name="Support For PLA" only generic-pla's material_exact matches.
  // Real-world Spoolman entries for support filaments set material to the
  // support product name itself (see docs/bambu-materials/README.md), so
  // the representative case is covered by
  // testResolveSupportMaterialsOutrankGeneric() above -- this test instead
  // locks in that name_contains_any alone never substitutes for a
  // material_exact condition when the latter is present on a rule.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("generic-pla", result.rule->id);
}

void testResolveUnsupportedMaterials() {
  const auto table = buildTestRuleTable();
  for (const char* material : {"Carbon Fiber", "Wood", "PC/ABS", "PVB"}) {
    const auto result =
        services::resolveBambuMaterialRule(table, {material, "", ""});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(services::BambuMaterialResolveStatus::Unsupported),
        static_cast<int>(result.status));
    TEST_ASSERT_NOT_NULL(result.rule);
    TEST_ASSERT_TRUE(result.rule->result.reason[0] != '\0');
  }
}

void testResolveUnknownMaterialIsNoMatch() {
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"Unobtainium", "", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::NoMatch),
                        static_cast<int>(result.status));
  TEST_ASSERT_NULL(result.rule);
}

void testResolvePriorityWinsOverGeneric() {
  // material=PLA, name=Silk PLA matches both generic-pla (priority 10)
  // and generic-pla-silk (priority 100) simultaneously -- the higher
  // priority must win, never JSON declaration order.
  const auto table = buildTestRuleTable();
  const auto result =
      services::resolveBambuMaterialRule(table, {"PLA", "Silk PLA", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                        static_cast<int>(result.status));
  TEST_ASSERT_EQUAL_STRING("generic-pla-silk", result.rule->id);
  TEST_ASSERT_EQUAL_INT32(100, result.matchedPriority);
}

void testResolveAmbiguousRulesAreReported() {
  // Two rules tied at the same highest priority must never be resolved by
  // "first match wins" -- this is a configuration error to surface, not
  // silently paper over.
  JsonDocument document;
  const auto parseError = deserializeJson(document, R"({
    "schema_version": 2,
    "rules": [
      {"id": "rule-a", "priority": 100, "match": {"material_exact": ["AMBIGUOUS-TEST"]},
       "result": {"status": "mapped", "tray_info_idx": "GFX01", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "rule-b", "priority": 100, "match": {"material_exact": ["AMBIGUOUS-TEST"]},
       "result": {"status": "mapped", "tray_info_idx": "GFX02", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}},
      {"id": "rule-lower", "priority": 10, "match": {"material_exact": ["AMBIGUOUS-TEST"]},
       "result": {"status": "mapped", "tray_info_idx": "GFX03", "tray_type": "PLA",
                  "nozzle_temp_min": 190, "nozzle_temp_max": 240}}
    ]
  })");
  TEST_ASSERT_FALSE(parseError);
  models::BambuMaterialRuleTable table{};
  const auto parseResult = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(parseResult.error));

  const auto result =
      services::resolveBambuMaterialRule(table, {"AMBIGUOUS-TEST", "", ""});
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Ambiguous),
                        static_cast<int>(result.status));
  TEST_ASSERT_NULL(result.rule);
  TEST_ASSERT_EQUAL_INT32(100, result.matchedPriority);
  TEST_ASSERT_EQUAL_STRING("rule-a,rule-b", result.ambiguousRuleIds);
}

void testResolveRegressionMaterials() {
  // Task description section 32: these must keep resolving correctly
  // through the schema-v2 migration.
  const auto table = buildTestRuleTable();
  struct Case {
    const char* material;
    const char* trayInfoIdx;
  };
  const Case cases[] = {
      {"PLA", "GFL99"},       {"PETG", "GFG99"},     {"ABS", "GFB99"},
      {"PLA-CF", "GFL98"},    {"PETG-CF", "GFG98"},  {"PA-CF", "GFN98"},
      {"PVA", "GFS99"},
  };
  for (const auto& testCase : cases) {
    const auto result =
        services::resolveBambuMaterialRule(table, {testCase.material, "", ""});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING(testCase.trayInfoIdx, result.rule->result.trayInfoIdx);
  }
}

void testResolveEmptyAndNullInputsAreNoMatch() {
  const auto table = buildTestRuleTable();
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialResolveStatus::NoMatch),
      static_cast<int>(
          services::resolveBambuMaterialRule(table, {"", "", ""}).status));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialResolveStatus::NoMatch),
      static_cast<int>(
          services::resolveBambuMaterialRule(table, {nullptr, nullptr, nullptr})
              .status));
}

// ---------------------------------------------------------------------
// Real shipped catalog (data/bambu-materials/bambu_materials.json)
// ---------------------------------------------------------------------

/// @brief Loads and parses the actual repository catalog file -- run from
///        the project root by `pio test`, same working directory
///        assumption other native tests in this project don't need since
///        they build their tables in-memory; this is the one exception,
///        specifically to guard the real shipped file against regressions.
models::BambuMaterialRuleTable loadShippedCatalog() {
  std::ifstream file("data/bambu-materials/bambu_materials.json");
  TEST_ASSERT_TRUE_MESSAGE(file.is_open(),
                           "data/bambu-materials/bambu_materials.json not "
                           "found -- run tests from the project root");
  std::stringstream buffer;
  buffer << file.rdbuf();
  const std::string content = buffer.str();

  JsonDocument document;
  const auto parseError = deserializeJson(document, content);
  TEST_ASSERT_FALSE(parseError);

  models::BambuMaterialRuleTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  return table;
}

void testShippedCatalogParsesAndResolvesAllThirtyThreeSpoolmanDbMaterials() {
  const auto table = loadShippedCatalog();
  TEST_ASSERT_GREATER_THAN_UINT16(0, table.ruleCount);

  struct Case {
    const char* material;
    bool mapped;
  };
  // The 33 current SpoolmanDB material types (task description section 13),
  // each of which must resolve to either Mapped or Unsupported -- never
  // NoMatch/Ambiguous, since that would mean the shipped catalog silently
  // fails to classify a material Spoolman itself offers.
  const Case cases[] = {
      {"PLA", true},          {"PLA+", true},
      {"ABS", true},          {"ABS+", true},
      {"ABS-T", true},        {"PETG", true},
      {"Nylon", true},        {"Flexible (TPU)", true},
      {"Polycarbonate (PC)", true}, {"PCTG", true},
      {"HIPS", true},         {"PVA", true},
      {"ASA", true},          {"Polypropylene (PP)", true},
      {"Wood", false},        {"Carbon Fiber", false},
      {"PC/ABS", false},      {"PC/PBT", false},
      {"Acetal (POM)", false}, {"PMMA", false},
      {"Semi flexible (FPE)", false}, {"PVDF", false},
      {"PEI (Ultem)", false}, {"PEKK", false},
      {"PEEK", false},        {"PPSU", false},
      {"BIOFUSION", false},   {"GREENTEC", false},
      {"FLAX", false},        {"PEARL", false},
      {"Flexible (TPE 32D)", false}, {"Flexible (TPE 88A)", false},
      {"PVB", false},
  };
  for (const auto& testCase : cases) {
    const auto result =
        services::resolveBambuMaterialRule(table, {testCase.material, "", ""});
    const bool resolvedDefinitively =
        result.status == services::BambuMaterialResolveStatus::Mapped ||
        result.status == services::BambuMaterialResolveStatus::Unsupported;
    TEST_ASSERT_TRUE_MESSAGE(resolvedDefinitively, testCase.material);
    const bool isMapped =
        result.status == services::BambuMaterialResolveStatus::Mapped;
    TEST_ASSERT_EQUAL_MESSAGE(testCase.mapped, isMapped, testCase.material);
  }
}

void testShippedCatalogSpecialProfilesAndSupportMaterials() {
  const auto table = loadShippedCatalog();

  auto expectMapped = [&](const char* material, const char* name,
                          const char* manufacturer, const char* trayInfoIdx) {
    const auto result = services::resolveBambuMaterialRule(
        table, {material, name, manufacturer});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialResolveStatus::Mapped),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING(trayInfoIdx, result.rule->result.trayInfoIdx);
  };

  expectMapped("PLA", "Super Silk PLA", "", "GFL96");
  expectMapped("PLA", "PLA High Speed", "", "GFL95");
  expectMapped("PLA+", "High Speed PLA+", "", "GFL95");
  expectMapped("PETG", "PETG HF", "", "GFG96");
  expectMapped("PETG", "PETG Basic", "", "GFG99");
  expectMapped("PLA", "PLA Basic", "Bambu Lab", "GFA00");
  expectMapped("PLA", "PLA Basic", "Other", "GFL99");

  expectMapped("Support For PLA", "", "", "GFS02");
  expectMapped("Support For PLA/PETG", "", "", "GFS05");
  expectMapped("Support For ABS", "", "", "GFS06");
  expectMapped("Support For PA/PET", "", "", "GFS03");
  expectMapped("Bambu PVA", "", "", "GFS04");
}

void testShippedCatalogNoAmbiguityAcrossAllRules() {
  // Every rule's own match values must not accidentally tie with another
  // rule at the same priority -- resolve each rule against its own first
  // material_exact value (plus any name_contains_any/manufacturer_exact
  // condition it requires) and confirm it wins outright.
  const auto table = loadShippedCatalog();
  for (std::uint16_t index = 0; index < table.ruleCount; ++index) {
    const models::BambuMaterialRule& rule = table.rules[index];
    // Extract the first '|'-separated value of each category, if any.
    auto firstValue = [](const char* joined) {
      std::string text(joined);
      const auto separatorPos = text.find(models::kBambuMatchValueSeparator);
      return separatorPos == std::string::npos ? text
                                               : text.substr(0, separatorPos);
    };
    const std::string material = rule.match.materialExact[0] != '\0'
                                     ? firstValue(rule.match.materialExact)
                                     : "";
    const std::string name = rule.match.nameContainsAny[0] != '\0'
                                 ? firstValue(rule.match.nameContainsAny)
                                 : "";
    const std::string manufacturer =
        rule.match.manufacturerExact[0] != '\0'
            ? firstValue(rule.match.manufacturerExact)
            : "";
    const auto result = services::resolveBambuMaterialRule(
        table, {material.c_str(), name.c_str(), manufacturer.c_str()});
    TEST_ASSERT_NOT_EQUAL_MESSAGE(
        static_cast<int>(services::BambuMaterialResolveStatus::Ambiguous),
        static_cast<int>(result.status), rule.id);
  }
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testValidSchemaV2WithMultipleRules);
  RUN_TEST(testInvalidJsonIsRejected);
  RUN_TEST(testMissingSchemaVersionIsRejected);
  RUN_TEST(testWrongSchemaVersionIsRejected);
  RUN_TEST(testMissingRulesArrayIsRejected);
  RUN_TEST(testEmptyRulesArrayIsValid);
  RUN_TEST(testMissingRuleIdIsRejected);
  RUN_TEST(testDuplicateRuleIdIsRejected);
  RUN_TEST(testMissingPriorityIsRejected);
  RUN_TEST(testEmptyMatchIsRejected);
  RUN_TEST(testInvalidStatusIsRejected);
  RUN_TEST(testMappedWithoutTrayInfoIdxIsRejected);
  RUN_TEST(testInvalidTemperatureRangeIsRejected);
  RUN_TEST(testUnsupportedWithoutReasonIsRejected);
  RUN_TEST(testWrongDataTypeInMatchArrayIsRejected);
  RUN_TEST(testErrorNamesAreStable);

  RUN_TEST(testResolveGenericPla);
  RUN_TEST(testResolveGenericPlaPlus);
  RUN_TEST(testResolveGenericAbsVariants);
  RUN_TEST(testResolvePlaSilk);
  RUN_TEST(testResolvePlaHighSpeed);
  RUN_TEST(testResolvePlaPlusHighSpeed);
  RUN_TEST(testResolvePetgHf);
  RUN_TEST(testResolveNormalPetg);
  RUN_TEST(testResolveBambuPlaBasic);
  RUN_TEST(testResolveNonBambuPlaBasicFallsBackToGeneric);
  RUN_TEST(testResolveSupportMaterialsOutrankGeneric);
  RUN_TEST(testResolveSupportForPlaDoesNotFallBackToGenericPla);
  RUN_TEST(testResolveUnsupportedMaterials);
  RUN_TEST(testResolveUnknownMaterialIsNoMatch);
  RUN_TEST(testResolvePriorityWinsOverGeneric);
  RUN_TEST(testResolveAmbiguousRulesAreReported);
  RUN_TEST(testResolveRegressionMaterials);
  RUN_TEST(testResolveEmptyAndNullInputsAreNoMatch);

  RUN_TEST(testShippedCatalogParsesAndResolvesAllThirtyThreeSpoolmanDbMaterials);
  RUN_TEST(testShippedCatalogSpecialProfilesAndSupportMaterials);
  RUN_TEST(testShippedCatalogNoAmbiguityAcrossAllRules);
  return UNITY_END();
}
