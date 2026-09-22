// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/environment_map.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include "linkage_check.h"

#include <cstdint>
#include <iostream>
#include <span>
#include <utility>

namespace {

granit_matrix4 identity() {
  granit_matrix4 value{};
  value.elements[0] = 1.0F;
  value.elements[5] = 1.0F;
  value.elements[10] = 1.0F;
  value.elements[15] = 1.0F;
  return value;
}

granit_result record_stage(const granit_render_pipeline_record_info* info, void* user_data) {
  if (info == nullptr || info->view == nullptr || user_data == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto renderer = *static_cast<const granit_renderer*>(user_data);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = info->depth_output;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  if (info->depth_output != GRANIT_NULL_HANDLE)
    rendering.depth_stencil_attachment = &depth;
  if (info->stage == GRANIT_RENDER_PIPELINE_STAGE_SHADOW) {
    rendering.area = {0, 0, 1024, 1024};
  } else {
    color.view = info->color_output;
    color.clear_value = {.red = 0.125F, .green = 0.25F, .blue = 0.5F, .alpha = 1.0F};
    rendering.color_attachment_count = 1;
    rendering.color_attachments = &color;
    rendering.area = {0, 0, static_cast<std::uint32_t>(info->view->viewport_width),
                      static_cast<std::uint32_t>(info->view->viewport_height)};
  }
  auto result = granit_command_recorder_begin_rendering(renderer, info->recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(renderer, info->recorder);
  return result;
}

} // namespace

int main() {
  granit::render_pipeline pipeline;
  granit::scene_snapshot scene;
  granit::material_instance material;
  granit::mesh mesh;
  if (pipeline.valid() || scene.valid() || material.valid() || mesh.valid() ||
      granit::material_parameter_id("base_color") == 0)
    return 1;
  granit::render_pipeline_desc desc{};
  if (pipeline.initialize(granit::renderer_ref{}, desc) != granit::result::invalid_handle ||
      pipeline.valid())
    return 2;

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize();
  if (renderer_result == granit::result::backend_unavailable ||
      renderer_result == granit::result::incompatible_driver ||
      renderer_result == granit::result::no_suitable_device)
    return renderer.valid() ? 3 : 0;
  if (renderer_result.failed())
    return 4;
  auto native_renderer = renderer.native_handle();
  desc.record = record_stage;
  desc.user_data = &native_renderer;
  if ((pipeline.initialize(renderer.ref(), desc)).failed())
    return 5;

  constexpr std::uint32_t size = 4;
  granit::texture output;
  granit::texture_view output_view;
  if (output.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                   .usage = granit::texture_usage::color_attachment |
                                            granit::texture_usage::transfer_source,
                                   .width = size,
                                   .height = size}) != granit::result::success ||
      output_view.initialize(renderer, output) != granit::result::success)
    return 6;

  granit_scene_view view{};
  view.view = identity();
  view.projection = identity();
  view.view_projection = identity();
  view.viewport_width = static_cast<float>(size);
  view.viewport_height = static_cast<float>(size);
  view.layer_mask = UINT64_MAX;
  if ((scene.initialize(renderer, {.views = std::span{&view, 1}})).failed())
    return 7;

  const granit::render_pipeline_render_desc render_desc{
      .scene = scene.ref(),
      .output = output_view.ref(),
      .output_format = granit::texture_format::rgba8_unorm,
      .width = size,
      .height = size,
  };
  const auto render_result = pipeline.render(render_desc);
  if (render_result.failed() && render_result != granit::result::not_ready) {
    std::cerr << "RenderPipeline实际渲染失败：" << static_cast<granit_result>(render_result)
              << '\n';
    return 8;
  }

  granit::render_pipeline moved = std::move(pipeline);
  if (pipeline.valid() || !moved.valid())
    return 9;
  if ((scene.reset()).failed())
    return 10;
  if ((moved.reset()).failed() || (moved.reset()).failed())
    return 11;
  granit::environment_map environment;
  if (environment.initialize_builtin(renderer).failed())
    return 12;
  granit::environment_map_info environment_info{};
  if (environment.get_info(environment_info).failed() || !environment_info.environment.irradiance)
    return 13;
  if (environment.reset().failed())
    return 14;
  return (renderer.reset()).failed() ? 15 : 0;
}
