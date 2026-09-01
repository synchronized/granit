// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char
    granit_pipeline_record_info_v1_size[sizeof(granit_render_pipeline_record_info) ==
                                                GRANIT_RENDER_PIPELINE_RECORD_INFO_VERSION_1_SIZE
                                            ? 1
                                            : -1];
typedef char granit_pipeline_desc_v1_size
    [sizeof(granit_render_pipeline_desc) == GRANIT_RENDER_PIPELINE_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_pipeline_output_v1_size[sizeof(granit_render_pipeline_output) ==
                                                    GRANIT_RENDER_PIPELINE_OUTPUT_VERSION_1_SIZE
                                                ? 1
                                                : -1];
typedef char
    granit_pipeline_render_desc_v1_size[sizeof(granit_render_pipeline_render_desc) ==
                                                GRANIT_RENDER_PIPELINE_RENDER_DESC_VERSION_1_SIZE
                                            ? 1
                                            : -1];
typedef char granit_pipeline_metrics_v1_size[sizeof(granit_render_pipeline_metrics) ==
                                                     GRANIT_RENDER_PIPELINE_METRICS_VERSION_1_SIZE
                                                 ? 1
                                                 : -1];

static granit_render_pipeline_output output = GRANIT_RENDER_PIPELINE_OUTPUT_INIT;
static granit_render_pipeline_render_desc render = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
static granit_render_pipeline_metrics metrics = GRANIT_RENDER_PIPELINE_METRICS_INIT;

void granit_pipeline_render_pipeline_h_compile(void) {
  (void)output;
  (void)render;
  (void)metrics;
}
