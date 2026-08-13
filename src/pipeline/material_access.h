// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MATERIAL_ACCESS_H_
#define GRANIT_PIPELINE_MATERIAL_ACCESS_H_

#include <granit/pipeline/material.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/resource_types.h>

namespace granit::pipeline::detail {

struct material_draw_request {
  uint64_t pass = 0;
  uint64_t variant = 0;
  granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  granit_texture_format depth_stencil_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  granit_sample_count sample_count = GRANIT_SAMPLE_COUNT_1;
};

/** Material 内部 GPU 对象的短期借用快照；仅在调用方保持 Material 存活时有效。 */
struct material_draw_state {
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  granit_pipeline_layout pipeline_layout = GRANIT_NULL_HANDLE;
  granit_bind_group_layout frame_layout = GRANIT_NULL_HANDLE;
  granit_bind_group_layout material_layout = GRANIT_NULL_HANDLE;
  granit_bind_group_layout object_layout = GRANIT_NULL_HANDLE;
  granit_bind_group_layout lighting_layout = GRANIT_NULL_HANDLE;
  granit_bind_group material_group = GRANIT_NULL_HANDLE;
};

[[nodiscard]] granit_result validate_material_handle(granit_renderer renderer,
                                                     granit_material material) noexcept;

[[nodiscard]] granit_result acquire_material_draw_state(
    granit_renderer renderer, granit_material material, const material_draw_request& request,
    material_draw_state& state) noexcept;

} // namespace granit::pipeline::detail

#endif
