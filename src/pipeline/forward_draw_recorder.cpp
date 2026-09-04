// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include "pipeline/forward_draw_recorder.h"
#include "material/material_package.h"
#include "pipeline/draw_binding_cache.h"
#include "pipeline/material_access.h"
#include "pipeline/mesh_access.h"
#include <array>
#include <granit/renderer/render_target.h>
#include <new>
#include <vector>
namespace granit::pipeline::detail {
namespace {
constexpr lighting::light_limits automatic_light_limits{.directional = 4, .point = 128, .spot = 64};
} // namespace
granit_result
record_opaque_draws(render_pipeline_state& state, granit_command_recorder recorder,
                    granit_texture_view color, granit_texture_view resolve_color,
                    granit_texture_view depth, granit_texture_view shadow, uint32_t width,
                    uint32_t height, const granit::material::pbr_frame_constants& frame,
                    std::span<const granit::material::pbr_object_constants> objects,
                    std::span<const granit_render_pipeline_draw_binding> draws,
                    const granit::lighting::packed_view_lights& lights,
                    const granit::lighting::shadow_sampling_constants& shadow_constants,
                    granit::lighting::ibl_texture_views ibl_views,
                    const granit::lighting::ibl_sampling_constants& ibl_constants,
                    bool use_uniform_arena, granit_clear_color_value clear_color) {
  if (shadow == GRANIT_NULL_HANDLE || objects.size() != draws.size())
    return GRANIT_ERROR_NOT_READY;
  granit_color_attachment_desc color_attachment = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color_attachment.view = color;
  color_attachment.clear_value = clear_color;
  granit_depth_stencil_attachment_desc depth_attachment = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth_attachment.view = depth;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color_attachment;
  rendering.depth_stencil_attachment = &depth_attachment;
  rendering.area = {0, 0, width, height};
  auto result = GRANIT_SUCCESS;
  const granit_viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const granit_scissor scissor{0, 0, width, height};
  std::vector<granit::pipeline::detail::material_draw_state> arena_materials;
  std::vector<granit::pipeline::detail::dynamic_uniform_binding> arena_bindings;
  if (use_uniform_arena) {
    result = release_legacy_uniform_bindings(state.opaque_draw_bindings);
    if (result != GRANIT_SUCCESS)
      return result;
    try {
      arena_materials.resize(draws.size());
      arena_bindings.resize(draws.size());
      std::vector<granit::pipeline::detail::dynamic_uniform_request> requests;
      requests.reserve(draws.size());
      for (std::size_t index = 0; index < draws.size(); ++index) {
        result = granit::pipeline::detail::acquire_material_draw_state(
            state.renderer, draws[index].material,
            {.pass = granit::material::make_feature_id("opaque"),
             .variant = 0,
             .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
             .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT,
             .sample_count = resolve_color == GRANIT_NULL_HANDLE ? GRANIT_SAMPLE_COUNT_1
                                                                 : GRANIT_SAMPLE_COUNT_4},
            arena_materials[index]);
        if (result != GRANIT_SUCCESS)
          break;
        requests.push_back({.material = &arena_materials[index],
                            .frame = std::as_bytes(std::span{&frame, 1}),
                            .object = std::as_bytes(std::span{&objects[index], 1})});
      }
      if (result == GRANIT_SUCCESS)
        result = state.uniform_arena.prepare_batch(requests, arena_bindings);
    } catch (const std::bad_alloc&) {
      result = GRANIT_ERROR_OUT_OF_MEMORY;
    }
  }
  for (size_t index = 0; result == GRANIT_SUCCESS && index < draws.size(); ++index) {
    auto material = use_uniform_arena ? arena_materials[index]
                                      : granit::pipeline::detail::material_draw_state{};
    if (!use_uniform_arena) {
      result = granit::pipeline::detail::acquire_material_draw_state(
          state.renderer, draws[index].material,
          {.pass = granit::material::make_feature_id("opaque"),
           .variant = 0,
           .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
           .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT,
           .sample_count =
               resolve_color == GRANIT_NULL_HANDLE ? GRANIT_SAMPLE_COUNT_1 : GRANIT_SAMPLE_COUNT_4},
          material);
    }
    if (index == state.opaque_draw_bindings.size())
      state.opaque_draw_bindings.emplace_back();
    auto& cached = state.opaque_draw_bindings[index];
    const bool ibl_changed =
        cached.ibl_views.irradiance != ibl_views.irradiance ||
        cached.ibl_views.prefiltered_environment != ibl_views.prefiltered_environment ||
        cached.ibl_views.brdf_lut != ibl_views.brdf_lut;
    if (result == GRANIT_SUCCESS && (cached.material != draws[index].material || ibl_changed ||
                                     (!use_uniform_arena && !cached.bindings.initialized()))) {
      result = cached.bindings.reset();
      if (result == GRANIT_SUCCESS)
        result = cached.lighting.reset();
      if (result == GRANIT_SUCCESS && !use_uniform_arena) {
        result = cached.bindings.initialize(state.renderer, material, frame, objects[index]);
      }
      if (result == GRANIT_SUCCESS) {
        result = cached.lighting.initialize(
            state.renderer, {.shadow = shadow, .ibl = ibl_views}, shadow_constants, ibl_constants,
            automatic_light_limits, {}, material.lighting_layout, granit::memory_location::upload);
      }
      if (result == GRANIT_SUCCESS)
        cached.material = draws[index].material;
      if (result == GRANIT_SUCCESS)
        cached.ibl_views = ibl_views;
    } else if (result == GRANIT_SUCCESS && !use_uniform_arena) {
      result = cached.bindings.update(frame, objects[index]);
    }
    if (result == GRANIT_SUCCESS)
      result = cached.lighting.update_shadow(shadow_constants);
    if (result == GRANIT_SUCCESS)
      result = cached.lighting.update_ibl(ibl_constants);
    if (result == GRANIT_SUCCESS)
      result = cached.lighting.update_lights(lights);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_bind_graphics_pipeline(state.renderer, recorder,
                                                              material.pipeline);
    const auto arena_binding = use_uniform_arena
                                   ? arena_bindings[index]
                                   : granit::pipeline::detail::dynamic_uniform_binding{};
    const std::array groups{
        use_uniform_arena ? arena_binding.frame_group : cached.bindings.frame_group(),
        material.material_group,
        use_uniform_arena ? arena_binding.object_group : cached.bindings.object_group(),
        cached.lighting.group()};
    const std::array<uint32_t, 2> dynamic_offsets{
        use_uniform_arena ? arena_binding.frame_offset : 0,
        use_uniform_arena ? arena_binding.object_offset : 0};
    if (result == GRANIT_SUCCESS) {
      const granit_bind_groups_desc bind_desc{GRANIT_BIND_GROUPS_DESC_VERSION_1_SIZE,
                                              0,
                                              groups.data(),
                                              static_cast<uint32_t>(groups.size()),
                                              static_cast<uint32_t>(dynamic_offsets.size()),
                                              dynamic_offsets.data()};
      result = granit_command_recorder_bind_graphics_groups(state.renderer, recorder,
                                                            material.pipeline_layout, &bind_desc);
    }
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_viewports(state.renderer, recorder, 0, &viewport, 1);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_scissors(state.renderer, recorder, 0, &scissor, 1);
    if (result == GRANIT_SUCCESS)
      result =
          granit::pipeline::detail::bind_mesh_buffers(state.renderer, recorder, draws[index].mesh);
    if (result == GRANIT_SUCCESS) {
      const bool final_draw = index + 1 == draws.size();
      color_attachment.resolve_view = final_draw ? resolve_color : GRANIT_NULL_HANDLE;
      color_attachment.store_operation = final_draw && resolve_color != GRANIT_NULL_HANDLE
                                             ? GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD
                                             : GRANIT_ATTACHMENT_STORE_OPERATION_STORE;
      if (index != 0) {
        color_attachment.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD;
        depth_attachment.depth_load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD;
      }
      result = granit_command_recorder_begin_rendering(state.renderer, recorder, &rendering);
      if (result == GRANIT_SUCCESS) {
        const auto draw_result =
            granit::pipeline::detail::draw_mesh(state.renderer, recorder, draws[index].mesh);
        const auto end_result = granit_command_recorder_end_rendering(state.renderer, recorder);
        result = draw_result == GRANIT_SUCCESS ? end_result : draw_result;
      }
    }
  }
  if (result == GRANIT_SUCCESS)
    result = trim_draw_binding_cache(state.opaque_draw_bindings, draws.size());
  return result;
}

}
