// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/ui_pass.h"

#include "material/material_package.h"
#include "pipeline/material_access.h"
#include "pipeline/pbr_draw_bindings.h"

#include <array>

namespace granit::pipeline::detail {

granit_result record_ui_pass(granit_renderer renderer, granit_command_recorder recorder,
                             const ui_pass_desc& desc, const ui_draw_list& list,
                             const ui_geometry_upload& geometry) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      desc.color == GRANIT_NULL_HANDLE || desc.color_format == GRANIT_TEXTURE_FORMAT_UNDEFINED ||
      desc.width == 0 || desc.height == 0 || desc.material == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (list.items().empty())
    return GRANIT_SUCCESS;
  if (geometry.vertex_buffer() == GRANIT_NULL_HANDLE ||
      geometry.index_buffer() == GRANIT_NULL_HANDLE ||
      geometry.vertex_count() != list.vertices().size() ||
      geometry.index_count() != list.indices().size()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto batches = list.batches();
  for (const auto& batch : batches) {
    if (batch.state.texture != GRANIT_NULL_HANDLE || batch.state.sampler != GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_UNSUPPORTED;
  }

  material_draw_state material;
  auto result =
      acquire_material_draw_state(renderer, desc.material,
                                  {.pass = granit::material::make_feature_id("unlit_ui"),
                                   .color_format = desc.color_format,
                                   .depth_stencil_format = GRANIT_TEXTURE_FORMAT_UNDEFINED},
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
        static_cast<std::uint32_t>(groups.size()));
  }
  const granit_vertex_buffer_binding vertex_binding{geometry.vertex_buffer(), 0};
  if (result == GRANIT_SUCCESS) {
    result = granit_command_recorder_bind_vertex_buffers(renderer, recorder, 0, &vertex_binding, 1);
  }
  if (result == GRANIT_SUCCESS) {
    result = granit_command_recorder_bind_index_buffer(renderer, recorder, geometry.index_buffer(),
                                                       0, GRANIT_INDEX_TYPE_UINT32);
  }
  const granit_viewport viewport{
      0, 0, static_cast<float>(desc.width), static_cast<float>(desc.height), 0, 1};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_viewports(renderer, recorder, 0, &viewport, 1);

  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = desc.color;
  color.load_operation = desc.load_operation;
  color.clear_value.alpha = 0.0F;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area = {0, 0, desc.width, desc.height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
  if (result == GRANIT_SUCCESS) {
    for (const auto& batch : batches) {
      const auto scissor = batch.state.scissor.width == 0 || batch.state.scissor.height == 0
                               ? granit_scissor{0, 0, desc.width, desc.height}
                               : batch.state.scissor;
      result = granit_command_recorder_set_scissors(renderer, recorder, 0, &scissor, 1);
      if (result == GRANIT_SUCCESS) {
        result = granit_command_recorder_draw_indexed(renderer, recorder, batch.index_count, 1,
                                                      batch.first_index, 0, 0);
      }
      if (result != GRANIT_SUCCESS)
        break;
    }
    const auto end_result = granit_command_recorder_end_rendering(renderer, recorder);
    if (result == GRANIT_SUCCESS)
      result = end_result;
  }
  const auto reset_result = bindings.reset();
  return result == GRANIT_SUCCESS ? reset_result : result;
}

} // namespace granit::pipeline::detail
