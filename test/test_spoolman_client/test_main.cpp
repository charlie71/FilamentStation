#include <unity.h>

#include <cstdio>
#include <cstring>

#include "services/SpoolmanClient.h"
#include "models/AppState.h"
#include "services/TagAssignmentPolicy.h"

using filament_station::services::SpoolmanClient;
using filament_station::services::SpoolmanHttpTransport;
using filament_station::services::TagExtraFieldStatus;
using filament_station::services::TagLookupStatus;
using filament_station::services::AssignmentDecision;
using filament_station::services::RemovalDecision;
using filament_station::services::NativeConsistency;
using filament_station::services::LegacyMigrationDecision;

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

void testDecodeNumberExtraField() {
  // Spoolman number extra fields are stored the same way as text fields --
  // the outer JSON value is a string whose *content* is itself JSON (here
  // a bare number, not a quoted string like the tag field).
  JsonDocument response;
  deserializeJson(response,
                  R"({"extra":{"bambu_temp_min":"190","bambu_temp_max":"220.5","bogus":"\"not-a-number\""}})");
  float value = 0.0F;
  TEST_ASSERT_TRUE(SpoolmanClient::decodeNumberExtraField(
      response["extra"]["bambu_temp_min"], value));
  TEST_ASSERT_EQUAL_FLOAT(190.0F, value);
  TEST_ASSERT_TRUE(SpoolmanClient::decodeNumberExtraField(
      response["extra"]["bambu_temp_max"], value));
  TEST_ASSERT_EQUAL_FLOAT(220.5F, value);
  TEST_ASSERT_FALSE(SpoolmanClient::decodeNumberExtraField(
      response["extra"]["bogus"], value));
  TEST_ASSERT_FALSE(SpoolmanClient::decodeNumberExtraField(
      response["extra"]["missing"], value));
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

void testAssignmentWorkflowDecisions() {
  using filament_station::services::assignmentDecision;
  TEST_ASSERT_EQUAL(AssignmentDecision::Blocked,
                    assignmentDecision(false, TagLookupStatus::NotFound, 0,
                                       42));
  TEST_ASSERT_EQUAL(AssignmentDecision::Assign,
                    assignmentDecision(true, TagLookupStatus::NotFound, 0,
                                       42));
  TEST_ASSERT_EQUAL(AssignmentDecision::Idempotent,
                    assignmentDecision(true, TagLookupStatus::Found, 42, 42));
  TEST_ASSERT_EQUAL(AssignmentDecision::Reassign,
                    assignmentDecision(true, TagLookupStatus::Found, 17, 42));
  TEST_ASSERT_EQUAL(AssignmentDecision::Duplicate,
                    assignmentDecision(true, TagLookupStatus::Duplicate, 0,
                                       42));
}

void testRemovalWorkflowDecisions() {
  using filament_station::services::removalDecision;
  TEST_ASSERT_EQUAL(RemovalDecision::Blocked,
                    removalDecision(false, TagLookupStatus::Found, 42));
  TEST_ASSERT_EQUAL(RemovalDecision::NotAssigned,
                    removalDecision(true, TagLookupStatus::NotFound, 0));
  TEST_ASSERT_EQUAL(RemovalDecision::Remove,
                    removalDecision(true, TagLookupStatus::Found, 42));
  TEST_ASSERT_EQUAL(RemovalDecision::Duplicate,
                    removalDecision(true, TagLookupStatus::Duplicate, 0));
}

void testOnlineAndExtraFieldRequirements() {
  using filament_station::services::tagOperationsAvailable;
  TEST_ASSERT_FALSE(tagOperationsAvailable(false, false));
  TEST_ASSERT_FALSE(tagOperationsAvailable(true, false));
  TEST_ASSERT_FALSE(tagOperationsAvailable(false, true));
  TEST_ASSERT_TRUE(tagOperationsAvailable(true, true));
}

void testNativePayloadConsistencyDecisions() {
  using filament_station::services::nativeConsistency;
  TEST_ASSERT_EQUAL(NativeConsistency::Unavailable,
                    nativeConsistency(false, TagLookupStatus::Found, 42, 42));
  TEST_ASSERT_EQUAL(
      NativeConsistency::MissingServerAssignment,
      nativeConsistency(true, TagLookupStatus::NotFound, 42, 0));
  TEST_ASSERT_EQUAL(NativeConsistency::Consistent,
                    nativeConsistency(true, TagLookupStatus::Found, 42, 42));
  TEST_ASSERT_EQUAL(
      NativeConsistency::ConflictingSpool,
      nativeConsistency(true, TagLookupStatus::Found, 42, 17));
  TEST_ASSERT_EQUAL(NativeConsistency::Duplicate,
                    nativeConsistency(true, TagLookupStatus::Duplicate, 42,
                                      0));
}

void testLegacyMigrationConflictPolicy() {
  using filament_station::services::legacyMigrationDecision;
  TEST_ASSERT_EQUAL(
      LegacyMigrationDecision::SetTarget,
      legacyMigrationDecision(true, "", "04A211FE428061"));
  TEST_ASSERT_EQUAL(
      LegacyMigrationDecision::AlreadyMigrated,
      legacyMigrationDecision(true, "04A211FE428061", "04A211FE428061"));
  TEST_ASSERT_EQUAL(
      LegacyMigrationDecision::Conflict,
      legacyMigrationDecision(true, "A1B2C3D4", "04A211FE428061"));
  TEST_ASSERT_EQUAL(
      LegacyMigrationDecision::Conflict,
      legacyMigrationDecision(false, "", "04A211FE428061"));
}

void testSpoolmanAppStateTransitionsAndPermissions() {
  using filament_station::models::SpoolmanAppState;
  using filament_station::models::spoolmanAppState;
  using filament_station::models::spoolmanOperationsAvailable;
  using filament_station::models::spoolmanTagOperationsAvailable;

  const auto offline = spoolmanAppState(false, false);
  TEST_ASSERT_EQUAL(SpoolmanAppState::SpoolmanUnavailable, offline);
  TEST_ASSERT_FALSE(spoolmanOperationsAvailable(offline));
  TEST_ASSERT_FALSE(spoolmanTagOperationsAvailable(offline));

  const auto withoutTagField = spoolmanAppState(true, false);
  TEST_ASSERT_EQUAL(SpoolmanAppState::TagFieldUnavailable, withoutTagField);
  TEST_ASSERT_TRUE(spoolmanOperationsAvailable(withoutTagField));
  TEST_ASSERT_FALSE(spoolmanTagOperationsAvailable(withoutTagField));

  const auto ready = spoolmanAppState(true, true);
  TEST_ASSERT_EQUAL(SpoolmanAppState::SpoolmanReady, ready);
  TEST_ASSERT_TRUE(spoolmanOperationsAvailable(ready));
  TEST_ASSERT_TRUE(spoolmanTagOperationsAvailable(ready));

  TEST_ASSERT_EQUAL(SpoolmanAppState::SpoolmanUnavailable,
                    spoolmanAppState(false, true));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testEnsureExistingTextField);
  RUN_TEST(testEnsureCreatesMissingField);
  RUN_TEST(testEnsureRejectsWrongTypeAndReportsTransportError);
  RUN_TEST(testDecodeTextExtraField);
  RUN_TEST(testDecodeNumberExtraField);
  RUN_TEST(testFindSpoolByTagStatuses);
  RUN_TEST(testFindRejectsInvalidIdentityAndHttpFailure);
  RUN_TEST(testSetAndClearSpoolTagEncodingAndVerification);
  RUN_TEST(testSetRequiresVerifiedServerResponse);
  RUN_TEST(testAssignmentWorkflowDecisions);
  RUN_TEST(testRemovalWorkflowDecisions);
  RUN_TEST(testOnlineAndExtraFieldRequirements);
  RUN_TEST(testNativePayloadConsistencyDecisions);
  RUN_TEST(testLegacyMigrationConflictPolicy);
  RUN_TEST(testSpoolmanAppStateTransitionsAndPermissions);
  return UNITY_END();
}
