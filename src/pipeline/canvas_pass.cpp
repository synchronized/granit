// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/canvas_pass.h"

#include "material/material_package.h"
#include "pipeline/material_access.h"
#include "pipeline/pbr_draw_bindings.h"

#include <array>

namespace granit::pipeline::detail {

granit_result record_canvas_pass(granit_renderer renderer, granit_command_recorder recorder,
                                 const canvas_pass_desc& desc, const canvas_draw_list& list,
                                 const canvas_geometry_upload& geometry,
                                 pbr_draw_bindings& bindings) noexcept {
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
    if (batch.state.texture == GRANIT_NULL_HANDLE || batch.state.sampler == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const auto update_material = [&](const canvas_draw_batch& batch) {
    const std::array updates{
        granit_material_parameter_update{granit_material_parameter_id("base_color_texture", 18),
                                         GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                         batch.state.texture},
        granit_material_parameter_update{granit_material_parameter_id("unlit_sampler", 13),
                                         GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0,
                                         batch.state.sampler}};
    return granit_material_update(renderer, desc.material, updates.data(),
                                  static_cast<std::uint32_t>(updates.size()));
  };
  auto result = update_material(batches.front());
  material_draw_state material;
  const material_draw_request request{
      .pass = granit::material::make_feature_id(desc.encode_srgb ? "unlit_canvas_encode_srgb"
                                                                 : "unlit_canvas"),
      .color_format = desc.color_format,
      .depth_stencil_format = GRANIT_TEXTURE_FORMAT_UNDEFINED};
  if (result == GRANIT_SUCCESS)
    result = acquire_material_draw_state(renderer, desc.material, request, material);
  if (result == GRANIT_SUCCESS)
    result = bindings.prepare(renderer, material, desc.frame, desc.object);
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
    auto current_texture = batches.front().state.texture;
    auto current_sampler = batches.front().state.sampler;
    for (const auto& batch : batches) {
      if (batch.state.texture != current_texture || batch.state.sampler != current_sampler) {
        // 资源绑定可能需要插入 Pipeline Barrier，必须暂时离开 Dynamic Rendering。
        result = granit_command_recorder_end_rendering(renderer, recorder);
        if (result == GRANIT_SUCCESS)
          result = update_material(batch);
        material_draw_state batch_material;
        if (result == GRANIT_SUCCESS)
          result = acquire_material_draw_state(renderer, desc.material, request, batch_material);
        if (result == GRANIT_SUCCESS) {
          result = granit_command_recorder_bind_graphics_groups(renderer, recorder,
                                                                batch_material.pipeline_layout, 1,
                                                                &batch_material.material_group, 1);
        }
        color.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD;
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
        current_texture = batch.state.texture;
        current_sampler = batch.state.sampler;
      }
      const auto scissor = batch.state.scissor.width == 0 || batch.state.scissor.height == 0
                               ? granit_scissor{0, 0, desc.width, desc.height}
                               : batch.state.scissor;
      if (result == GRANIT_SUCCESS)
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
  return result;
}

} // namespace granit::pipeline::detail
