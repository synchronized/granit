// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#ifndef GRANIT_PIPELINE_SHADOW_DRAW_RECORDER_H_
#define GRANIT_PIPELINE_SHADOW_DRAW_RECORDER_H_
#include "lighting/directional_shadow.h"
#include "pipeline/render_pipeline_state.h"
#include <granit/pipeline/render_pipeline.h>
#include <span>
namespace granit::pipeline::detail {
[[nodiscard]] granit_result record_shadow_draws(
    render_pipeline_state& state, granit_command_recorder recorder, granit_texture_view depth,
    const lighting::shadow_frame_constants& frame, std::span<const lighting::shadow_caster> casters,
    std::span<const granit_render_pipeline_draw_binding> draws, bool use_uniform_arena);
}
#endif
