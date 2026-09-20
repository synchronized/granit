// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.h>
#include <granit/window/window.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void installed_pipeline_rejects_invalid_renderer(void) {
  const granit_render_pipeline_desc desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  granit_render_pipeline pipeline = UINT64_C(1);
  TEST_ASSERT_EQUAL_INT32(GRANIT_ERROR_INVALID_HANDLE,
                          granit_render_pipeline_create(GRANIT_NULL_HANDLE, &desc, &pipeline));
  TEST_ASSERT_TRUE(pipeline == GRANIT_NULL_HANDLE);
}

static void installed_window_exposes_browser_backend(void) {
  TEST_ASSERT_EQUAL_UINT32(UINT32_C(4), GRANIT_WINDOW_BACKEND_EMSCRIPTEN);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(installed_pipeline_rejects_invalid_renderer);
  RUN_TEST(installed_window_exposes_browser_backend);
  return UNITY_END();
}
