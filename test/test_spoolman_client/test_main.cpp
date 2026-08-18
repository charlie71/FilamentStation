#include <unity.h>

#include <cstdio>
#include <cstring>

#include "services/SpoolmanClient.h"

using filament_station::services::SpoolmanClient;
using filament_station::services::SpoolmanHttpTransport;
using filament_station::services::TagExtraFieldStatus;
using filament_station::services::TagLookupStatus;

class MockTransport final : public SpoolmanHttpTransport {
 public:
  const char* getResponse = "[]";
  const char* postResponse = "[]";
  const char* patchResponse = "{}";
  bool getSuccess = true;
  bool postSuccess = true;
  bool patchSuccess = true;
  const char* failureText = "mock transport error";
  char lastPath[192]{};
  char lastRequest[256]{};

  bool get(const char* path, JsonDocument& response, char* error,
           std::size_t errorCapacity) override {
    std::snprintf(lastPath, sizeof(lastPath), "%s", path);
    return finish(getSuccess, getResponse, response, error, errorCapacity,
                  failureText);
  }

  bool post(const char* path, const JsonDocument& request,
            JsonDocument& response, char* error,
            std::size_t errorCapacity) override {
    std::snprintf(lastPath, sizeof(lastPath), "%s", path);
    serializeJson(request, lastRequest, sizeof(lastRequest));
    return finish(postSuccess, postResponse, response, error, errorCapacity,
                  failureText);
  }

  bool patch(const char* path, const JsonDocument& request,
             JsonDocument& response, char* error,
             std::size_t errorCapacity) override {
    std::snprintf(lastPath, sizeof(lastPath), "%s", path);
    serializeJson(request, lastRequest, sizeof(lastRequest));
    return finish(patchSuccess, patchResponse, response, error, errorCapacity,
                  failureText);
  }

 private:
  static bool finish(bool success, const char* body, JsonDocument& response,
                     char* error, std::size_t errorCapacity,
                     const char* failureText) {
    if (!success) {
      std::snprintf(error, errorCapacity, "%s", failureText);
      return false;
    }
    return !deserializeJson(response, body);
  }
};

void setUp() {}
void tearDown() {}

void testEnsureExistingTextField() {
  MockTransport transport;
  transport.getResponse = R"([{"key":"tag","name":"Tag","field_type":"text"}])";
  SpoolmanClient client(transport);
  char error[96]{};
  TEST_ASSERT_EQUAL(TagExtraFieldStatus::Available,
                    client.ensureTagExtraField(error, sizeof(error)));
  TEST_ASSERT_EQUAL_STRING("/field/spool", transport.lastPath);
  TEST_ASSERT_EQUAL_STRING("", error);
}

void testEnsureCreatesMissingField() {
  MockTransport transport;
  transport.postResponse =
      R"([{"key":"tag","name":"FilamentStation Tag","field_type":"text"}])";
  SpoolmanClient client(transport);
  char error[96]{};
  TEST_ASSERT_EQUAL(TagExtraFieldStatus::Created,
                    client.ensureTagExtraField(error, sizeof(error)));
  TEST_ASSERT_EQUAL_STRING("/field/spool/tag", transport.lastPath);
  TEST_ASSERT_NOT_NULL(std::strstr(transport.lastRequest,
                                   "\"field_type\":\"text\""));
}

void testEnsureRejectsWrongTypeAndReportsTransportError() {
  MockTransport transport;
  transport.getResponse = R"([{"key":"tag","field_type":"integer"}])";
  SpoolmanClient client(transport);
  char error[96]{};
  TEST_ASSERT_EQUAL(TagExtraFieldStatus::Incompatible,
                    client.ensureTagExtraField(error, sizeof(error)));
  transport.getSuccess = false;
  TEST_ASSERT_EQUAL(TagExtraFieldStatus::Error,
                    client.ensureTagExtraField(error, sizeof(error)));
  TEST_ASSERT_EQUAL_STRING("mock transport error", error);
}

void testDecodeTextExtraField() {
  JsonDocument response;
  deserializeJson(response, R"({"extra":{"tag":"\"04A2B3C4\""}})");
  char decoded[40]{};
  TEST_ASSERT_TRUE(SpoolmanClient::decodeTextExtraField(
      response["extra"]["tag"], decoded, sizeof(decoded)));
  TEST_ASSERT_EQUAL_STRING("04A2B3C4", decoded);
  response["extra"]["tag"] = "not-json";
  TEST_ASSERT_FALSE(SpoolmanClient::decodeTextExtraField(
      response["extra"]["tag"], decoded, sizeof(decoded)));
}

void testFindSpoolByTagStatuses() {
  MockTransport transport;
  SpoolmanClient client(transport);
  auto result = client.findSpoolByTag("04A2B3C4");
  TEST_ASSERT_EQUAL(TagLookupStatus::NotFound, result.status);
  TEST_ASSERT_NOT_NULL(std::strstr(transport.lastPath,
                                   "extra.tag=%2204A2B3C4%22"));

  transport.getResponse =
      R"([{"id":42,"extra":{"tag":"\"04A2B3C4\""}}])";
  result = client.findSpoolByTag("04A2B3C4");
  TEST_ASSERT_EQUAL(TagLookupStatus::Found, result.status);
  TEST_ASSERT_EQUAL_UINT32(42, result.spoolId);

  transport.getResponse =
      R"([{"id":42,"extra":{"tag":"\"04A2B3C4\""}},{"id":17,"extra":{"tag":"\"04A2B3C4\""}}])";
  result = client.findSpoolByTag("04A2B3C4");
  TEST_ASSERT_EQUAL(TagLookupStatus::Duplicate, result.status);
  TEST_ASSERT_EQUAL_UINT16(2, result.matches);
  TEST_ASSERT_EQUAL_UINT32(0, result.spoolId);
}

void testFindRejectsInvalidIdentityAndHttpFailure() {
  MockTransport transport;
  SpoolmanClient client(transport);
  TEST_ASSERT_EQUAL(TagLookupStatus::Error,
                    client.findSpoolByTag("04:a2").status);
  transport.getSuccess = false;
  const auto result = client.findSpoolByTag("04A2");
  TEST_ASSERT_EQUAL(TagLookupStatus::Error, result.status);
  TEST_ASSERT_EQUAL_STRING("mock transport error", result.error);
  transport.failureText = "Request timed out";
  const auto timeout = client.findSpoolByTag("04A2B3C4");
  TEST_ASSERT_EQUAL(TagLookupStatus::Error, timeout.status);
  TEST_ASSERT_EQUAL_STRING("Request timed out", timeout.error);
}

void testSetAndClearSpoolTagEncodingAndVerification() {
  MockTransport transport;
  SpoolmanClient client(transport);
  transport.patchResponse =
      R"({"id":42,"extra":{"tag":"\"04A2B3C4\""}})";
  auto result = client.setSpoolTag(42, "04A2B3C4");
  TEST_ASSERT_TRUE(result.success);
  TEST_ASSERT_EQUAL_STRING("/spool/42", transport.lastPath);
  TEST_ASSERT_NOT_NULL(
      std::strstr(transport.lastRequest, "\"tag\":\"\\\"04A2B3C4\\\"\""));

  transport.patchResponse = R"({"id":42,"extra":{}})";
  result = client.clearSpoolTag(42);
  TEST_ASSERT_TRUE(result.success);
  TEST_ASSERT_NOT_NULL(std::strstr(transport.lastRequest, "\"tag\":null"));
}

void testSetRequiresVerifiedServerResponse() {
  MockTransport transport;
  SpoolmanClient client(transport);
  transport.patchResponse =
      R"({"id":42,"extra":{"tag":"\"DIFFERENT\""}})";
  const auto result = client.setSpoolTag(42, "04A2B3C4");
  TEST_ASSERT_FALSE(result.success);
  TEST_ASSERT_EQUAL_STRING("Spool tag verification failed", result.error);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testEnsureExistingTextField);
  RUN_TEST(testEnsureCreatesMissingField);
  RUN_TEST(testEnsureRejectsWrongTypeAndReportsTransportError);
  RUN_TEST(testDecodeTextExtraField);
  RUN_TEST(testFindSpoolByTagStatuses);
  RUN_TEST(testFindRejectsInvalidIdentityAndHttpFailure);
  RUN_TEST(testSetAndClearSpoolTagEncodingAndVerification);
  RUN_TEST(testSetRequiresVerifiedServerResponse);
  return UNITY_END();
}
