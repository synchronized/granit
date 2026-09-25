// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>
#include <granit/pipeline/render_pipeline.h>
#include <granit/window.h>

#include <unity.h>

void setUp(void) {}

void tearDown(void) {}

static void granit_test_core_create_failures_clear_output(void) {
  granit_renderer renderer = UINT64_C(42);
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.reserved = UINT32_C(1);
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_renderer_create(&renderer_desc, &renderer));
  TEST_ASSERT_EQUAL_UINT64(GRANIT_NULL_HANDLE, renderer);

  granit_buffer buffer = UINT64_C(42);
  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.size = UINT64_C(64);
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  buffer_desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_buffer_create(GRANIT_NULL_HANDLE, &buffer_desc, &buffer));
  TEST_ASSERT_EQUAL_UINT64(GRANIT_NULL_HANDLE, buffer);
}

static void granit_test_pipeline_validates_description_before_owner(void) {
  granit_render_pipeline pipeline = UINT64_C(42);
  granit_render_pipeline_desc desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  desc.reserved = UINT32_C(1);
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_render_pipeline_create(UINT64_MAX, &desc, &pipeline));
  TEST_ASSERT_EQUAL_UINT64(GRANIT_NULL_HANDLE, pipeline);

  desc = (granit_render_pipeline_desc)GRANIT_RENDER_PIPELINE_DESC_INIT;
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_render_pipeline_create(GRANIT_NULL_HANDLE, &desc, &pipeline));
  TEST_ASSERT_EQUAL_UINT64(GRANIT_NULL_HANDLE, pipeline);
}

static void granit_test_window_create_failures_clear_output(void) {
  granit_window_system system = UINT64_C(42);
  granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  system_desc.reserved = UINT32_C(1);
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_window_system_create(&system_desc, &system));
  TEST_ASSERT_EQUAL_UINT64(GRANIT_NULL_HANDLE, system);

  granit_window window = UINT64_C(42);
  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = UINT32_C(64);
  window_desc.height = UINT32_C(64);
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_window_create(GRANIT_NULL_HANDLE, &window_desc, &window));
  TEST_ASSERT_EQUAL_UINT64(GRANIT_NULL_HANDLE, window);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(granit_test_core_create_failures_clear_output);
  RUN_TEST(granit_test_pipeline_validates_description_before_owner);
  RUN_TEST(granit_test_window_create_failures_clear_output);
  return UNITY_END();
}
