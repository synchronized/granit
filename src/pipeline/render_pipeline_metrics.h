// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_PIPELINE_METRICS_H
#define GRANIT_PIPELINE_RENDER_PIPELINE_METRICS_H

#include <granit/pipeline/render_pipeline.h>

extern "C" GRANIT_RENDER_PIPELINE_API granit_result granit_render_pipeline_shadow_half_extent_set(
    granit_renderer renderer, granit_render_pipeline pipeline, float half_extent);

#endif
