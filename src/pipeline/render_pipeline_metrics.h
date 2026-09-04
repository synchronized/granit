// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_PIPELINE_METRICS_H
#define GRANIT_PIPELINE_RENDER_PIPELINE_METRICS_H

#include <granit/pipeline/render_pipeline.h>

#include "pipeline/render_pipeline_state.h"

namespace granit::pipeline::detail {

[[nodiscard]] granit_result
publish_render_pipeline_metrics(render_pipeline_state& state,
                                render_pipeline_state::metrics_slot& slot);

[[nodiscard]] granit_timestamp_query_pool
prepare_render_pipeline_metrics_slot(render_pipeline_state& state, std::uint32_t frame_slot,
                                     std::uint32_t frame_slot_count);

} // namespace granit::pipeline::detail

extern "C" GRANIT_RENDER_PIPELINE_API granit_result granit_render_pipeline_shadow_half_extent_set(
    granit_renderer renderer, granit_render_pipeline pipeline, float half_extent);

#endif
