// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>
#include <unity.h>

void setUp(void) {}

void tearDown(void) {}

static void granit_test_public_types(void) {
  TEST_ASSERT_EQUAL_size_t(sizeof(uint64_t), sizeof(granit_handle));
  TEST_ASSERT_EQUAL_UINT64(0, GRANIT_NULL_HANDLE);
}

static void granit_test_result_messages(void) {
  TEST_ASSERT_NOT_NULL(granit_result_message(GRANIT_SUCCESS));
  TEST_ASSERT_NOT_NULL(granit_result_message(GRANIT_ERROR_INVALID_ARGUMENT));
}

static void granit_test_renderer_rejects_invalid_arguments(void) {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.api_version = 0;

  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT, granit_renderer_create(0, &renderer));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_renderer_create(&renderer_desc, &renderer));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE, granit_renderer_destroy(GRANIT_NULL_HANDLE));
}

static void granit_test_surface_rejects_invalid_arguments(void) {
  granit_surface surface = GRANIT_NULL_HANDLE;
  granit_win32_surface_desc desc = GRANIT_WIN32_SURFACE_DESC_INIT;

  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_surface_create_win32(GRANIT_NULL_HANDLE, &desc, &surface));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_surface_create_win32(UINT64_C(1), &desc, &surface));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_surface_destroy(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE));
}

static void granit_test_header_and_runtime_versions_match(void) {
  TEST_ASSERT_EQUAL_UINT32(GRANIT_VERSION_MAJOR, granit_version_major());
  TEST_ASSERT_EQUAL_UINT32(GRANIT_VERSION_MINOR, granit_version_minor());
  TEST_ASSERT_EQUAL_UINT32(GRANIT_VERSION_PATCH, granit_version_patch());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(granit_test_public_types);
  RUN_TEST(granit_test_result_messages);
  RUN_TEST(granit_test_renderer_rejects_invalid_arguments);
  RUN_TEST(granit_test_surface_rejects_invalid_arguments);
  RUN_TEST(granit_test_header_and_runtime_versions_match);
  return UNITY_END();
}
