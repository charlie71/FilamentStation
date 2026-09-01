#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "services/JsonStorage.h"

namespace {

using filament_station::rtos::StorageDocumentType;
using filament_station::services::JsonStorage;
using filament_station::services::JsonStorageError;

class BufferPrint final : public Print {
 public:
  std::size_t write(std::uint8_t value) override {
    if (length_ >= sizeof(buffer_) - 1U) {
      return 0;
    }
    buffer_[length_++] = static_cast<char>(value);
    buffer_[length_] = '\0';
    return 1;
  }

  const char* data() const { return buffer_; }

 private:
  char buffer_[256]{};
  std::size_t length_ = 0;
};

void test_defaults_create_valid_envelope() {
  JsonDocument document;
  document.to<JsonObject>();

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(JsonStorage::applyDefaults(document)));
  TEST_ASSERT_EQUAL_UINT32(1, document["schemaVersion"].as<std::uint32_t>());
  TEST_ASSERT_EQUAL_STRING("1970-01-01T00:00:00Z",
                           document["updatedAt"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(JsonStorageError::Ok),
                        static_cast<int>(JsonStorage::validate(document)));
}

void test_non_object_root_is_rejected() {
  JsonDocument document;
  document.to<JsonArray>();

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::RootNotObject),
      static_cast<int>(JsonStorage::applyDefaults(document)));
}

void test_unsupported_schema_is_rejected() {
  JsonDocument document;
  document["schemaVersion"] = 2;
  document["updatedAt"] = "2026-08-03T12:00:00Z";

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::UnsupportedSchemaVersion),
      static_cast<int>(JsonStorage::validate(document)));
}

void test_invalid_timestamp_is_rejected() {
  JsonDocument document;
  document["schemaVersion"] = 1;
  document["updatedAt"] = "2026-08-03 12:00:00";

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidUpdatedAt),
      static_cast<int>(JsonStorage::validate(document)));
}

void test_valid_document_serializes() {
  JsonDocument document;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(JsonStorage::createDefault(
          StorageDocumentType::Device, document)));
  document["value"] = 42;
  BufferPrint output;

  const auto result =
      JsonStorage::serialize(document, StorageDocumentType::Device, output);
  TEST_ASSERT_TRUE(result.ok());
  TEST_ASSERT_GREATER_THAN_UINT32(0, result.bytesProcessed);
  TEST_ASSERT_NOT_NULL(strstr(output.data(), "\"schemaVersion\":1"));
}

void test_initial_document_defaults_validate() {
  constexpr StorageDocumentType types[] = {
      StorageDocumentType::Device, StorageDocumentType::Network,
      StorageDocumentType::Spoolman, StorageDocumentType::Bambu,
      StorageDocumentType::Ui,
      StorageDocumentType::Scale, StorageDocumentType::Nfc};
  for (const StorageDocumentType type : types) {
    JsonDocument document;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(JsonStorageError::Ok),
        static_cast<int>(JsonStorage::createDefault(type, document)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(JsonStorageError::Ok),
        static_cast<int>(JsonStorage::validate(document, type)));
  }
}

void test_bambu_defaults_support_multiple_printers() {
  JsonDocument document;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(JsonStorage::createDefault(
          StorageDocumentType::Bambu, document)));
  TEST_ASSERT_EQUAL_UINT16(0,
                           document["selectedPrinterId"].as<std::uint16_t>());
  TEST_ASSERT_EQUAL_UINT16(0,
                           document["defaultPrinterId"].as<std::uint16_t>());
  TEST_ASSERT_TRUE(document["printers"].is<JsonArrayConst>());
  TEST_ASSERT_EQUAL_UINT32(0, document["printers"].size());
}

JsonObject addBambuPrinter(JsonDocument& document, std::uint16_t printerId,
                           const char* name, const char* host,
                           const char* serialNumber, const char* accessCode,
                           bool enabled, bool isDefault, bool isSelected) {
  JsonArray printers = document["printers"].as<JsonArray>();
  JsonObject printer = printers.add<JsonObject>();
  printer["printerId"] = printerId;
  printer["name"] = name;
  printer["host"] = host;
  printer["serialNumber"] = serialNumber;
  printer["accessCode"] = accessCode;
  printer["enabled"] = enabled;
  printer["default"] = isDefault;
  printer["selected"] = isSelected;
  return printer;
}

void test_bambu_single_printer_document_is_valid() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "192.168.10.50", "01P00A123456789",
                  "12345678", true, true, true);
  document["defaultPrinterId"] = 1;
  document["selectedPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_printer_without_selection_is_valid() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "192.168.10.50", "01P00A123456789",
                  "12345678", true, true, false);
  document["defaultPrinterId"] = 1;
  document["selectedPrinterId"] = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_duplicate_printer_id_is_rejected() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "192.168.10.50", "01P00A123456789",
                  "12345678", true, true, false);
  addBambuPrinter(document, 1, "Drucker 2", "192.168.10.51", "01P00A987654321",
                  "87654321", true, false, false);
  document["defaultPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_missing_default_is_rejected() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "192.168.10.50", "01P00A123456789",
                  "12345678", true, false, false);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_multiple_defaults_is_rejected() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "192.168.10.50", "01P00A123456789",
                  "12345678", true, true, false);
  addBambuPrinter(document, 2, "Drucker 2", "192.168.10.51", "01P00A987654321",
                  "87654321", true, true, false);
  document["defaultPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_selected_printer_must_match_flag_and_id() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "192.168.10.50", "01P00A123456789",
                  "12345678", true, true, false);
  document["defaultPrinterId"] = 1;
  document["selectedPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));

  document["printers"][0]["selected"] = true;
  document["selectedPrinterId"] = 2;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_printer_count_exceeds_maximum_is_rejected() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  for (std::uint16_t index = 1; index <= 5; ++index) {
    addBambuPrinter(document, index, "Drucker", "192.168.10.50",
                    "01P00A123456789", "12345678", true, index == 1, false);
  }
  document["defaultPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_empty_fields_are_rejected() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "", "192.168.10.50", "01P00A123456789",
                  "12345678", true, true, false);
  document["defaultPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));

  document["printers"][0]["name"] = "Drucker 1";
  document["printers"][0]["host"] = "";
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_bambu_host_accepts_ipv4_and_hostname() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Bambu, document);
  addBambuPrinter(document, 1, "Drucker 1", "bambu-x1", "01P00A123456789",
                  "12345678", true, true, false);
  document["defaultPrinterId"] = 1;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));

  document["printers"][0]["host"] = "999.1.1.1";
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Bambu)));
}

void test_scale_defaults_and_calibration_validation() {
  JsonDocument document;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(JsonStorage::createDefault(
          StorageDocumentType::Scale, document)));
  TEST_ASSERT_FALSE(document["calibrated"].as<bool>());
  TEST_ASSERT_EQUAL_INT32(0, document["tareOffsetCounts"].as<std::int32_t>());
  TEST_ASSERT_FLOAT_WITHIN(
      0.0001F, 1.0F, document["factorCountsPerGram"].as<float>());

  document["calibrated"] = true;
  document["factorCountsPerGram"] = 0.0F;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Scale)));
}

void test_network_defaults_are_complete_and_valid() {
  JsonDocument document;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(JsonStorage::createDefault(
          StorageDocumentType::Network, document)));
  TEST_ASSERT_EQUAL_STRING("filamentstation",
                           document["hostname"].as<const char*>());
  TEST_ASSERT_TRUE(document["dhcp"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("", document["ipAddress"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("", document["gateway"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("", document["subnetMask"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("", document["dns"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("FilamentStation",
                           document["portalName"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT16(
      180, document["portalTimeoutSeconds"].as<std::uint16_t>());
  TEST_ASSERT_EQUAL_UINT16(
      20, document["connectTimeoutSeconds"].as<std::uint16_t>());
}

void test_static_network_configuration_validates_ipv4_fields() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Network, document);
  document["dhcp"] = false;
  document["ipAddress"] = "192.168.10.42";
  document["gateway"] = "192.168.10.1";
  document["subnetMask"] = "255.255.255.0";
  document["dns"] = "1.1.1.1";
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Network)));

  document["ipAddress"] = "192.168.10.999";
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Network)));
}

void test_network_names_and_timeouts_are_bounded() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Network, document);
  document["hostname"] = "-invalid";
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Network)));

  document["hostname"] = "filamentstation";
  document["portalName"] = "12345678901234567890123456";
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Network)));

  document["portalName"] = "FilamentStation";
  document["portalTimeoutSeconds"] = 29;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentField),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Network)));
}

void test_document_type_mismatch_is_rejected() {
  JsonDocument document;
  JsonStorage::createDefault(StorageDocumentType::Device, document);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::InvalidDocumentType),
      static_cast<int>(
          JsonStorage::validate(document, StorageDocumentType::Network)));
}

void test_native_ntag_mapping_document_is_valid() {
  JsonDocument document;
  const auto parseError = deserializeJson(
      document,
      R"({"schemaVersion":1,"updatedAt":"1970-01-01T00:00:00Z","documentType":"nfc","tagSchemaVersion":1,"mappings":[{"uid":"04B0008B780000","format":"filamentStation","spoolId":91}]})");
  TEST_ASSERT_FALSE(parseError);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(JsonStorageError::Ok),
      static_cast<int>(JsonStorage::validate(document,
                                             StorageDocumentType::Nfc)));
}

}  // namespace

void setup() {
  Serial.begin(filament_station::config::kSerialBaudRate);
  Serial.setTxTimeoutMs(
      filament_station::config::kUsbCdcTransmitTimeoutMs);
  vTaskDelay(pdMS_TO_TICKS(
      filament_station::config::kUsbCdcStartupDelayMs));

  UNITY_BEGIN();
  RUN_TEST(test_defaults_create_valid_envelope);
  RUN_TEST(test_non_object_root_is_rejected);
  RUN_TEST(test_unsupported_schema_is_rejected);
  RUN_TEST(test_invalid_timestamp_is_rejected);
  RUN_TEST(test_valid_document_serializes);
  RUN_TEST(test_initial_document_defaults_validate);
  RUN_TEST(test_bambu_defaults_support_multiple_printers);
  RUN_TEST(test_bambu_single_printer_document_is_valid);
  RUN_TEST(test_bambu_printer_without_selection_is_valid);
  RUN_TEST(test_bambu_duplicate_printer_id_is_rejected);
  RUN_TEST(test_bambu_missing_default_is_rejected);
  RUN_TEST(test_bambu_multiple_defaults_is_rejected);
  RUN_TEST(test_bambu_selected_printer_must_match_flag_and_id);
  RUN_TEST(test_bambu_printer_count_exceeds_maximum_is_rejected);
  RUN_TEST(test_bambu_empty_fields_are_rejected);
  RUN_TEST(test_bambu_host_accepts_ipv4_and_hostname);
  RUN_TEST(test_scale_defaults_and_calibration_validation);
  RUN_TEST(test_network_defaults_are_complete_and_valid);
  RUN_TEST(test_static_network_configuration_validates_ipv4_fields);
  RUN_TEST(test_network_names_and_timeouts_are_bounded);
  RUN_TEST(test_document_type_mismatch_is_rejected);
  RUN_TEST(test_native_ntag_mapping_document_is_valid);
  UNITY_END();
}

void loop() { vTaskDelay(portMAX_DELAY); }
