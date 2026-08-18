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
  RUN_TEST(test_scale_defaults_and_calibration_validation);
  RUN_TEST(test_document_type_mismatch_is_rejected);
  RUN_TEST(test_native_ntag_mapping_document_is_valid);
  UNITY_END();
}

void loop() { vTaskDelay(portMAX_DELAY); }
