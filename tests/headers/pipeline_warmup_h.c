// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline_warmup.h>

static granit_pipeline_warmup_batch_desc desc = GRANIT_PIPELINE_WARMUP_BATCH_DESC_INIT;
static granit_pipeline_warmup_batch_info info = GRANIT_PIPELINE_WARMUP_BATCH_INFO_INIT;
static granit_pipeline_warmup_result_info result = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;

int granit_pipeline_warmup_h_compiles(void) {
  return (int)(desc.struct_size + info.struct_size + result.struct_size);
}
