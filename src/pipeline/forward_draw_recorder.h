// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_FORWARD_DRAW_RECORDER_H_
#define GRANIT_PIPELINE_FORWARD_DRAW_RECORDER_H_
#include "lighting/light_data.h"
#include "lighting/shadow_resources.h"
#include "material/pbr_types.h"
#include "pipeline/render_pipeline_state.h"
#include <granit/pipeline/render_pipeline.h>
#include <span>
namespace granit::pipeline::detail {
[[nodiscard]] granit_result record_opaque_draws(
    render_pipeline_state& state, granit_command_recorder recorder, granit_texture_view color,
    granit_texture_view resolve_color, granit_texture_view depth, granit_texture_view shadow,
    std::uint32_t width, std::uint32_t height, const material::pbr_frame_constants& frame,
    std::span<const material::pbr_object_constants> objects,
    std::span<const granit_render_pipeline_draw_binding> draws,
    const lighting::packed_view_lights& lights,
    const lighting::shadow_sampling_constants& shadow_constants,
    lighting::ibl_texture_views ibl_views, const lighting::ibl_sampling_constants& ibl_constants,
    bool use_uniform_arena, granit_clear_color_value clear_color);
}
#endif
