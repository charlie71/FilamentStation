#include <ArduinoJson.h>
#include <unity.h>

#include "services/BambuMaterialCatalog.h"

using namespace filament_station;

void setUp() {}
void tearDown() {}

namespace {

void testValidFileWithMultipleMaterialsAndAliases() {
  JsonDocument document;
  const auto parseError = deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {
        "material": "PLA",
        "tray_info_idx": "GFL99",
        "tray_type": "PLA",
        "nozzle_temp_min": 190,
        "nozzle_temp_max": 240,
        "aliases": ["PLA"]
      },
      {
        "material": "PLA-CF",
        "tray_info_idx": "GFL98",
        "tray_type": "PLA-CF",
        "nozzle_temp_min": 190,
        "nozzle_temp_max": 240,
        "aliases": ["PLA CF", "PLACF", "PLA_CF"]
      },
      {
        "material": "PETG",
        "tray_info_idx": "GFG99",
        "tray_type": "PETG",
        "nozzle_temp_min": 220,
        "nozzle_temp_max": 260,
        "aliases": []
      }
    ]
  })");
  TEST_ASSERT_FALSE(parseError);

  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  TEST_ASSERT_EQUAL_UINT16(3, table.entryCount);
  TEST_ASSERT_EQUAL_UINT32(1, table.schemaVersion);
  TEST_ASSERT_EQUAL_STRING("PLA", table.entries[0].material);
  TEST_ASSERT_EQUAL_STRING("GFL99", table.entries[0].trayInfoIdx);
  TEST_ASSERT_EQUAL_STRING("PLA-CF", table.entries[1].material);
  TEST_ASSERT_EQUAL_STRING("PLA CF|PLACF|PLA_CF", table.entries[1].aliases);
  TEST_ASSERT_EQUAL_STRING("PETG", table.entries[2].material);
  TEST_ASSERT_EQUAL_STRING("", table.entries[2].aliases);
}

void testValidFileWithIndentationAndBlankLines() {
  // Formatting (indentation, blank lines) must not affect parsing --
  // exercises the same document with deliberately loose whitespace.
  JsonDocument document;
  const auto parseError = deserializeJson(document, R"(
    {

      "schema_version": 1,

      "materials": [
        {
          "material": "PLA",

          "tray_info_idx": "GFL99",
          "tray_type": "PLA",
          "nozzle_temp_min": 190,
          "nozzle_temp_max": 240

        }
      ]

    }
  )");
  TEST_ASSERT_FALSE(parseError);

  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  TEST_ASSERT_EQUAL_UINT16(1, table.entryCount);
}

void testInvalidJsonIsRejected() {
  JsonDocument document;
  const auto parseError = deserializeJson(document, R"({"schema_version": 1, "materials": [)");
  TEST_ASSERT_TRUE(parseError);
  // Not passed to parseBambuMaterialCatalog() at all in production (see
  // StorageTask.cpp::loadAndValidateBambuMaterialFile()) -- a document that
  // failed to deserialize is never handed to the catalog parser. This test
  // instead exercises passing a non-object root, the catalog parser's own
  // "invalid JSON shape" rejection.
  JsonDocument arrayRoot;
  deserializeJson(arrayRoot, R"(["not", "an", "object"])");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(arrayRoot, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::InvalidJson),
                        static_cast<int>(result.error));
}

void testMissingSchemaVersionIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({"materials": []})");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingSchemaVersion),
      static_cast<int>(result.error));
}

void testUnsupportedSchemaVersionIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({"schema_version": 2, "materials": []})");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::UnsupportedSchemaVersion),
      static_cast<int>(result.error));
}

void testMissingRequiredFieldIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {"material": "PLA", "tray_type": "PLA", "nozzle_temp_min": 190, "nozzle_temp_max": 240}
    ]
  })");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingRequiredField),
      static_cast<int>(result.error));
}

void testWrongFieldTypeForTemperatureIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {"material": "PLA", "tray_info_idx": "GFL99", "tray_type": "PLA",
       "nozzle_temp_min": "190", "nozzle_temp_max": 240}
    ]
  })");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::InvalidTemperatureRange),
      static_cast<int>(result.error));
}

void testInvertedTemperatureRangeIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {"material": "PLA", "tray_info_idx": "GFL99", "tray_type": "PLA",
       "nozzle_temp_min": 240, "nozzle_temp_max": 190}
    ]
  })");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::InvalidTemperatureRange),
      static_cast<int>(result.error));
}

void testInvalidAliasTypeIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {"material": "PLA", "tray_info_idx": "GFL99", "tray_type": "PLA",
       "nozzle_temp_min": 190, "nozzle_temp_max": 240, "aliases": "PLA"}
    ]
  })");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::InvalidAliasType),
      static_cast<int>(result.error));
}

void testDuplicateMaterialKeyIsRejected() {
  // Same example as docs/bambu-protocol.md section 27: "PLA-BASIC"'s alias
  // "PLA" collides with the separate "PLA" entry's own material name.
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {"material": "PLA", "tray_info_idx": "GFL99", "tray_type": "PLA",
       "nozzle_temp_min": 190, "nozzle_temp_max": 240, "aliases": []},
      {"material": "PLA-BASIC", "tray_info_idx": "GFL98", "tray_type": "PLA",
       "nozzle_temp_min": 190, "nozzle_temp_max": 240, "aliases": ["PLA"]}
    ]
  })");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::DuplicateLookupKey),
      static_cast<int>(result.error));
}

void testAliasRestatingOwnMaterialIsAllowed() {
  // An alias that merely spells its own entry's material under a different
  // separator style (e.g. "PLA CF" as an alias of material "PLA-CF", both
  // already equal under services::sameMaterialKey()) is harmless and
  // explicitly allowed -- not a DuplicateLookupKey, since it still
  // resolves to the same entry (docs/bambu-protocol.md example schema
  // uses exactly this pattern).
  JsonDocument document;
  deserializeJson(document, R"({
    "schema_version": 1,
    "materials": [
      {"material": "PLA-CF", "tray_info_idx": "GFL98", "tray_type": "PLA-CF",
       "nozzle_temp_min": 190, "nozzle_temp_max": 240,
       "aliases": ["PLA CF", "PLACF", "PLA_CF"]}
    ]
  })");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  TEST_ASSERT_EQUAL_UINT16(1, table.entryCount);
}

void testEmptyMaterialsArrayIsValid() {
  // An empty catalog is a valid (if useless) document -- distinct from a
  // missing/malformed "materials" field.
  JsonDocument document;
  deserializeJson(document, R"({"schema_version": 1, "materials": []})");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(services::BambuMaterialCatalogError::Ok),
                        static_cast<int>(result.error));
  TEST_ASSERT_EQUAL_UINT16(0, table.entryCount);
}

void testMissingMaterialsArrayIsRejected() {
  JsonDocument document;
  deserializeJson(document, R"({"schema_version": 1})");
  models::BambuMaterialMappingTable table{};
  const auto result = services::parseBambuMaterialCatalog(document, table);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(services::BambuMaterialCatalogError::MissingMaterialsArray),
      static_cast<int>(result.error));
}

void testErrorNamesAreStable() {
  TEST_ASSERT_EQUAL_STRING(
      "ok", services::bambuMaterialCatalogErrorName(
                services::BambuMaterialCatalogError::Ok));
  TEST_ASSERT_EQUAL_STRING(
      "duplicate_lookup_key",
      services::bambuMaterialCatalogErrorName(
          services::BambuMaterialCatalogError::DuplicateLookupKey));
  TEST_ASSERT_EQUAL_STRING(
      "unsupported_schema_version",
      services::bambuMaterialCatalogErrorName(
          services::BambuMaterialCatalogError::UnsupportedSchemaVersion));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testValidFileWithMultipleMaterialsAndAliases);
  RUN_TEST(testValidFileWithIndentationAndBlankLines);
  RUN_TEST(testInvalidJsonIsRejected);
  RUN_TEST(testMissingSchemaVersionIsRejected);
  RUN_TEST(testUnsupportedSchemaVersionIsRejected);
  RUN_TEST(testMissingRequiredFieldIsRejected);
  RUN_TEST(testWrongFieldTypeForTemperatureIsRejected);
  RUN_TEST(testInvertedTemperatureRangeIsRejected);
  RUN_TEST(testInvalidAliasTypeIsRejected);
  RUN_TEST(testDuplicateMaterialKeyIsRejected);
  RUN_TEST(testAliasRestatingOwnMaterialIsAllowed);
  RUN_TEST(testEmptyMaterialsArrayIsValid);
  RUN_TEST(testMissingMaterialsArrayIsRejected);
  RUN_TEST(testErrorNamesAreStable);
  return UNITY_END();
}
