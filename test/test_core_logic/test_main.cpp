#include <unity.h>

#include "core_logic.h"

void test_truncates_long_text_without_splitting_utf8() {
  TEST_ASSERT_EQUAL_STRING("0123456...", core::truncateUtf8("0123456789ABC", 10).c_str());
  TEST_ASSERT_EQUAL_STRING("Пр...", core::truncateUtf8("Привет", 7).c_str());
  TEST_ASSERT_EQUAL_STRING("short", core::truncateUtf8("short", 10).c_str());
}

void test_builds_short_github_url() {
  TEST_ASSERT_EQUAL_STRING("github.com/octocat",
                           core::shortGitHubUrl("octocat").c_str());
  TEST_ASSERT_EQUAL_STRING("github.com", core::shortGitHubUrl("").c_str());
}

void test_falls_back_for_empty_name_and_bio() {
  TEST_ASSERT_EQUAL_STRING("octocat",
                           core::displayName("", "octocat").c_str());
  TEST_ASSERT_EQUAL_STRING("The Octocat",
                           core::displayName("The Octocat", "octocat").c_str());
  TEST_ASSERT_TRUE(core::optionalBio("", 40).empty());
}

void test_limits_number_of_profiles() {
  TEST_ASSERT_EQUAL_UINT32(100, core::limitedProfileCount(145, 100));
  TEST_ASSERT_EQUAL_UINT32(12, core::limitedProfileCount(12, 100));
}

void test_retry_backoff_is_bounded() {
  TEST_ASSERT_EQUAL_UINT32(15000, core::retryDelay(0, 15000, 900000));
  TEST_ASSERT_EQUAL_UINT32(120000, core::retryDelay(3, 15000, 900000));
  TEST_ASSERT_EQUAL_UINT32(900000, core::retryDelay(20, 15000, 900000));
}

void runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_truncates_long_text_without_splitting_utf8);
  RUN_TEST(test_builds_short_github_url);
  RUN_TEST(test_falls_back_for_empty_name_and_bio);
  RUN_TEST(test_limits_number_of_profiles);
  RUN_TEST(test_retry_backoff_is_bounded);
  UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
  delay(1500);
  runAllTests();
}

void loop() {}
#else
int main(int, char**) {
  runAllTests();
  return 0;
}
#endif
