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
  TEST_ASSERT_NOT_NULL(granit_result_message(GRANIT_ERROR_NOT_READY));
}

static void granit_test_renderer_rejects_invalid_arguments(void) {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.api_version = 0;

  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT, granit_renderer_create(0, &renderer));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_renderer_create(&renderer_desc, &renderer));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE, granit_renderer_destroy(GRANIT_NULL_HANDLE));
  uint64_t cache_size = 0;
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_renderer_pipeline_cache_import(GRANIT_NULL_HANDLE, 0, 0));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_renderer_pipeline_cache_export(GRANIT_NULL_HANDLE, 0, &cache_size));
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

static void granit_test_swapchain_rejects_invalid_arguments(void) {
  granit_swapchain swapchain = GRANIT_NULL_HANDLE;
  granit_swapchain_desc desc = GRANIT_SWAPCHAIN_DESC_INIT;
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;

  TEST_ASSERT_EQUAL_INT(
      GRANIT_ERROR_INVALID_ARGUMENT,
      granit_swapchain_create(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, &desc, &swapchain));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_swapchain_recreate(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, &desc));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_swapchain_get_info(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, &info));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_swapchain_destroy(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE));
}

static void granit_test_buffer_rejects_invalid_arguments(void) {
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
  void* mapped = 0;

  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_buffer_create(GRANIT_NULL_HANDLE, &desc, &buffer));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_buffer_map(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, 0, 1, &mapped));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_buffer_unmap(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_buffer_destroy(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_buffer_create_with_data(GRANIT_NULL_HANDLE, &desc, 0, &buffer));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_buffer_write(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, 0, 0, 1));
}

static void granit_test_texture_copy_rejects_invalid_arguments(void) {
  granit_texture_copy_region region = {0};
  region.array_layer_count = 1;
  region.aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT;
  region.width = 1;
  region.height = 1;
  region.depth = 1;

  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_command_recorder_copy_texture(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE,
                                                             GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE,
                                                             &region));
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_HANDLE,
                        granit_command_recorder_copy_texture(
                            UINT64_C(1), UINT64_C(2), GRANIT_NULL_HANDLE, UINT64_C(4), &region));
}

static int granit_test_environment_unavailable(granit_result result) {
  return result == GRANIT_ERROR_BACKEND_UNAVAILABLE || result == GRANIT_ERROR_INCOMPATIBLE_DRIVER ||
         result == GRANIT_ERROR_NO_SUITABLE_DEVICE;
}

static void granit_test_command_recorder_batch_submit(void) {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  const granit_result create_result = granit_renderer_create(&renderer_desc, &renderer);
  if (granit_test_environment_unavailable(create_result)) {
    TEST_IGNORE_MESSAGE("当前运行环境没有满足要求的 Vulkan 设备");
  }
  TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS, create_result);

  granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  granit_command_recorder recorders[2] = {GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE};
  for (uint32_t index = 0; index < 2; ++index) {
    TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS, granit_command_recorder_create(renderer, &recorder_desc,
                                                                         &recorders[index]));
    TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS,
                          granit_command_recorder_begin(renderer, recorders[index]));
    TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS, granit_command_recorder_end(renderer, recorders[index]));
  }
  const granit_command_recorder duplicate[2] = {recorders[0], recorders[0]};
  TEST_ASSERT_EQUAL_INT(GRANIT_ERROR_INVALID_ARGUMENT,
                        granit_command_recorder_submit_batch(renderer, duplicate, 2));
  TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS,
                        granit_command_recorder_submit_batch(renderer, recorders, 2));
  for (uint32_t index = 0; index < 2; ++index) {
    TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS,
                          granit_command_recorder_reset(renderer, recorders[index]));
    TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS,
                          granit_command_recorder_destroy(renderer, recorders[index]));
  }
  TEST_ASSERT_EQUAL_INT(GRANIT_SUCCESS, granit_renderer_destroy(renderer));
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
  RUN_TEST(granit_test_swapchain_rejects_invalid_arguments);
  RUN_TEST(granit_test_buffer_rejects_invalid_arguments);
  RUN_TEST(granit_test_texture_copy_rejects_invalid_arguments);
  RUN_TEST(granit_test_command_recorder_batch_submit);
  RUN_TEST(granit_test_header_and_runtime_versions_match);
  return UNITY_END();
}
