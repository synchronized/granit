// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void version_matches_headers(void) {
  TEST_ASSERT_EQUAL_UINT32(GRANIT_VERSION_MAJOR, granit_version_major());
  TEST_ASSERT_EQUAL_UINT32(GRANIT_VERSION_MINOR, granit_version_minor());
  TEST_ASSERT_EQUAL_UINT32(GRANIT_VERSION_PATCH, granit_version_patch());
}

static void result_messages_cover_failure_and_unknown_codes(void) {
  TEST_ASSERT_EQUAL_STRING("success", granit_result_message(GRANIT_SUCCESS));
  TEST_ASSERT_EQUAL_STRING("invalid argument",
                           granit_result_message(GRANIT_ERROR_INVALID_ARGUMENT));
  TEST_ASSERT_EQUAL_STRING("operation cancelled", granit_result_message(GRANIT_ERROR_CANCELLED));
  TEST_ASSERT_EQUAL_STRING("unrecognized result", granit_result_message(INT32_C(-999)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(version_matches_headers);
  RUN_TEST(result_messages_cover_failure_and_unknown_codes);
  return UNITY_END();
}
