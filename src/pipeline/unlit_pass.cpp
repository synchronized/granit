// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/unlit_pass.h"

#include "material/material_package.h"
#include "pipeline/material_access.h"
#include "pipeline/mesh_access.h"
#include "pipeline/pbr_draw_bindings.h"

#include <array>

namespace granit::pipeline::detail {

granit_result record_unlit_pass(granit_renderer renderer, granit_command_recorder recorder,
                                const unlit_pass_desc& desc) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      desc.color == GRANIT_NULL_HANDLE || desc.color_format == GRANIT_TEXTURE_FORMAT_UNDEFINED ||
      desc.width == 0 || desc.height == 0 || desc.mesh == GRANIT_NULL_HANDLE ||
      desc.material == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  material_draw_state material;
  auto result =
      acquire_material_draw_state(renderer, desc.material,
                                  {.pass = granit::material::make_feature_id(
                                       desc.alpha_cutoff ? "unlit_alpha_cutoff" : "unlit_opaque"),
                                   .color_format = desc.color_format,
                                   .depth_stencil_format = desc.depth_format},
                                  material);
  pbr_draw_bindings bindings;
  if (result == GRANIT_SUCCESS)
    result = bindings.initialize(renderer, material, desc.frame, desc.object);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_bind_graphics_pipeline(renderer, recorder, material.pipeline);
  const std::array groups{bindings.frame_group(), material.material_group, bindings.object_group()};
  if (result == GRANIT_SUCCESS) {
    result = granit_command_recorder_bind_graphics_groups(
        renderer, recorder, material.pipeline_layout, 0, groups.data(),
        static_cast<uint32_t>(groups.size()));
  }
  if (result == GRANIT_SUCCESS)
    result = bind_mesh_buffers(renderer, recorder, desc.mesh);
  const granit_viewport viewport{
      0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height), 0, 1};
  const granit_scissor scissor{0, 0, desc.width, desc.height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_viewports(renderer, recorder, 0, &viewport, 1);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_scissors(renderer, recorder, 0, &scissor, 1);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = desc.color;
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = desc.depth;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.depth_stencil_attachment = desc.depth == GRANIT_NULL_HANDLE ? nullptr : &depth;
  rendering.area = {0, 0, desc.width, desc.height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
  if (result == GRANIT_SUCCESS) {
    const auto draw_result = draw_mesh(renderer, recorder, desc.mesh);
    const auto end_result = granit_command_recorder_end_rendering(renderer, recorder);
    result = draw_result == GRANIT_SUCCESS ? end_result : draw_result;
  }
  const auto reset_result = bindings.reset();
  return result == GRANIT_SUCCESS ? reset_result : result;
}

} // namespace granit::pipeline::detail
