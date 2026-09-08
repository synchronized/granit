// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.h>
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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(installed_pipeline_rejects_invalid_renderer);
  return UNITY_END();
}
