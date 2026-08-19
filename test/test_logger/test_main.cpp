#include <unity.h>

#include <cstring>
#include <cstdio>
#include <string>

#include "services/LoggerFormat.h"

using filament_station::services::LogComponent;
using filament_station::services::LogLevel;
using filament_station::services::Logger;
using filament_station::services::formatLogRecord;

extern "C" void setUp() {}
extern "C" void tearDown() {}

namespace {

void assertRecord(LogLevel level, LogComponent component,
                  const char* expectedPrefix) {
  char record[128]{};
  const auto length =
      formatLogRecord(record, sizeof(record), level, component, "event key=7");
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);
  TEST_ASSERT_EQUAL_STRING(expectedPrefix, record);
  TEST_ASSERT_EQUAL_CHAR('\n', record[length - 1]);
}

void test_all_levels_and_key_value_format() {
  assertRecord(LogLevel::Error, LogComponent::App, "E [APP] event key=7\n");
  assertRecord(LogLevel::Warn, LogComponent::App, "W [APP] event key=7\n");
  assertRecord(LogLevel::Info, LogComponent::App, "I [APP] event key=7\n");
  assertRecord(LogLevel::Debug, LogComponent::App, "D [APP] event key=7\n");
  assertRecord(LogLevel::Trace, LogComponent::App, "T [APP] event key=7\n");
}

void test_all_components() {
  const LogComponent components[] = {
      LogComponent::App,     LogComponent::Rtos,    LogComponent::Ui,
      LogComponent::Display, LogComponent::Touch,   LogComponent::Storage,
      LogComponent::Net,     LogComponent::Spoolman, LogComponent::Scale,
      LogComponent::Nfc,     LogComponent::Bambu};
  const char* names[] = {"APP", "RTOS", "UI", "DISPLAY", "TOUCH", "STORAGE",
                         "NET", "SPOOLMAN", "SCALE", "NFC", "BAMBU"};
  for (std::size_t index = 0; index < sizeof(components) / sizeof(components[0]);
       ++index) {
    char record[64]{};
    formatLogRecord(record, sizeof(record), LogLevel::Info, components[index],
                    "ok=true");
    char prefix[32]{};
    std::snprintf(prefix, sizeof(prefix), "I [%s] ", names[index]);
    TEST_ASSERT_EQUAL_STRING(prefix, std::string(record, std::strlen(prefix)).c_str());
  }
}

void test_embedded_newlines_are_flattened_and_one_newline_remains() {
  char record[128]{};
  const auto length = formatLogRecord(record, sizeof(record), LogLevel::Info,
                                      LogComponent::Nfc, "first\nsecond\r\nthird");
  TEST_ASSERT_EQUAL_STRING("I [NFC] first second  third\n", record);
  TEST_ASSERT_NULL(std::strchr(record, '\r'));
  TEST_ASSERT_EQUAL_CHAR('\n', record[length - 1]);
}

void test_long_message_is_truncated_but_record_remains_complete() {
  char message[512];
  std::memset(message, 'x', sizeof(message) - 1);
  message[sizeof(message) - 1] = '\0';
  char record[64]{};
  const auto length = formatLogRecord(record, sizeof(record), LogLevel::Debug,
                                      LogComponent::Ui, message);
  TEST_ASSERT_EQUAL_UINT32(sizeof(record) - 1, length);
  TEST_ASSERT_EQUAL_CHAR('\n', record[length - 1]);
  TEST_ASSERT_EQUAL_CHAR('\0', record[length]);
}

void test_compile_time_level_filter() {
  TEST_ASSERT_TRUE(Logger::enabled(LogLevel::Error));
#if FS_LOG_LEVEL >= 5
  TEST_ASSERT_TRUE(Logger::enabled(LogLevel::Trace));
#else
  TEST_ASSERT_FALSE(Logger::enabled(LogLevel::Trace));
#endif
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_all_levels_and_key_value_format);
  RUN_TEST(test_all_components);
  RUN_TEST(test_embedded_newlines_are_flattened_and_one_newline_remains);
  RUN_TEST(test_long_message_is_truncated_but_record_remains_complete);
  RUN_TEST(test_compile_time_level_filter);
  return UNITY_END();
}
