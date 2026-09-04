// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include "pipeline/shadow_draw_recorder.h"
#include "material/material_package.h"
#include "pipeline/draw_binding_cache.h"
#include "pipeline/material_access.h"
#include "pipeline/mesh_access.h"
#include <algorithm>
#include <array>
#include <granit/renderer/render_target.h>
#include <new>
#include <vector>
namespace granit::pipeline::detail {
granit_result acquire_shadow_pipeline(render_pipeline_state& state,
                                      const granit::pipeline::detail::material_draw_state& material,
                                      granit_mesh mesh, granit_graphics_pipeline& pipeline) {
  const auto cached = std::ranges::find_if(state.shadow_pipelines, [&](const auto& entry) {
    return entry.layout == material.pipeline_layout && entry.mesh == mesh;
  });
  if (cached != state.shadow_pipelines.end()) {
    pipeline = cached->pipeline;
    return GRANIT_SUCCESS;
  }
  granit::pipeline::detail::mesh_pipeline_state mesh_state;
  auto result =
      granit::pipeline::detail::copy_mesh_pipeline_state(state.renderer, mesh, mesh_state);
  if (result != GRANIT_SUCCESS)
    return result;
  std::vector<granit_vertex_buffer_layout> layouts;
  layouts.reserve(mesh_state.vertex_buffers.size());
  for (const auto& source : mesh_state.vertex_buffers) {
    layouts.push_back({source.stride, source.step_mode,
                       static_cast<uint32_t>(source.attributes.size()), 0,
                       source.attributes.data()});
  }
  const granit_depth_state depth_state{1, 1, GRANIT_COMPARE_OPERATION_LESS_EQUAL, 0};
  const granit_depth_bias_state depth_bias{1.25F, 1.75F, 0.0F, 0};
  granit_graphics_pipeline_desc desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  desc.layout = material.pipeline_layout;
  desc.vertex_shader = state.shadow_vertex_shader.native_handle();
  desc.fragment_shader = state.shadow_fragment_shader.native_handle();
  desc.depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
  desc.vertex_buffer_layout_count = static_cast<uint32_t>(layouts.size());
  desc.vertex_buffer_layouts = layouts.data();
  desc.primitive.topology = mesh_state.topology;
  desc.primitive.front_face = GRANIT_FRONT_FACE_CLOCKWISE;
  desc.primitive.cull_mode = GRANIT_CULL_MODE_BACK;
  desc.depth = &depth_state;
  desc.depth_bias = &depth_bias;
  granit_graphics_pipeline created = GRANIT_NULL_HANDLE;
  result = granit_graphics_pipeline_create(state.renderer, &desc, &created);
  if (result != GRANIT_SUCCESS)
    return result;
  try {
    state.shadow_pipelines.push_back({material.pipeline_layout, mesh, created});
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_graphics_pipeline_destroy(state.renderer, created));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  pipeline = created;
  return GRANIT_SUCCESS;
}

granit_result record_shadow_draws(render_pipeline_state& state, granit_command_recorder recorder,
                                  granit_texture_view depth,
                                  const granit::lighting::shadow_frame_constants& frame,
                                  std::span<const granit::lighting::shadow_caster> casters,
                                  std::span<const granit_render_pipeline_draw_binding> draws,
                                  bool use_uniform_arena) {
  if (casters.size() != draws.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_depth_stencil_attachment_desc depth_attachment = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth_attachment.view = depth;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.depth_stencil_attachment = &depth_attachment;
  rendering.area = {0, 0, 1024, 1024};
  auto result = GRANIT_SUCCESS;
  const granit_viewport viewport{0, 0, 1024, 1024, 0, 1};
  const granit_scissor scissor{0, 0, 1024, 1024};
  const granit::material::pbr_frame_constants unused_frame{};
  std::vector<granit::pipeline::detail::material_draw_state> arena_materials;
  std::vector<granit::material::pbr_object_constants> arena_objects;
  std::vector<granit::pipeline::detail::dynamic_uniform_binding> arena_bindings;
  if (use_uniform_arena) {
    result = release_legacy_uniform_bindings(state.shadow_draw_bindings);
    if (result != GRANIT_SUCCESS)
      return result;
    try {
      arena_materials.resize(draws.size());
      arena_objects.reserve(draws.size());
      arena_bindings.resize(draws.size());
      std::vector<granit::pipeline::detail::dynamic_uniform_request> requests;
      requests.reserve(draws.size());
      for (std::size_t index = 0; index < draws.size(); ++index) {
        result = granit::pipeline::detail::acquire_material_draw_state(
            state.renderer, draws[index].material,
            {.pass = granit::material::make_feature_id("opaque"),
             .variant = 0,
             .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
             .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
            arena_materials[index]);
        if (result != GRANIT_SUCCESS)
          break;
        arena_objects.push_back({.model = casters[index].model,
                                 .normal_matrix = granit::math::identity_matrix4,
                                 .object_id = {casters[index].object_id, 0, 0, 0}});
        requests.push_back({.material = &arena_materials[index],
                            .frame = std::as_bytes(std::span{&unused_frame, 1}),
                            .object = std::as_bytes(std::span{&arena_objects.back(), 1})});
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
           .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
          material);
    }
    if (result != GRANIT_SUCCESS)
      break;
    const auto object = use_uniform_arena ? arena_objects[index]
                                          : granit::material::pbr_object_constants{
                                                .model = casters[index].model,
                                                .normal_matrix = granit::math::identity_matrix4,
                                                .object_id = {casters[index].object_id, 0, 0, 0}};
    if (index == state.shadow_draw_bindings.size())
      state.shadow_draw_bindings.emplace_back();
    auto& cached = state.shadow_draw_bindings[index];
    if (result == GRANIT_SUCCESS && (cached.material != draws[index].material ||
                                     (!use_uniform_arena && !cached.bindings.initialized()))) {
      result = cached.bindings.reset();
      if (result == GRANIT_SUCCESS)
        result = cached.lighting.reset();
      if (result == GRANIT_SUCCESS && !use_uniform_arena) {
        result = cached.bindings.initialize(state.renderer, material, unused_frame, object);
      }
      if (result == GRANIT_SUCCESS) {
        result = cached.lighting.initialize(
            state.renderer,
            {.shadow = state.shadow_placeholder_view.native_handle(),
             .ibl = {.irradiance = state.default_ibl.irradiance(),
                     .prefiltered_environment = state.default_ibl.prefiltered_environment(),
                     .brdf_lut = state.default_ibl.brdf_lut()}},
            {.light_view_projection = frame.light_view_projection,
             .texel_size = {1.0F / 1024.0F, 1.0F / 1024.0F}},
            {.intensity = 0.0F}, {}, {}, material.lighting_layout, granit::memory_location::upload);
      }
      if (result == GRANIT_SUCCESS)
        cached.material = draws[index].material;
    } else if (result == GRANIT_SUCCESS && !use_uniform_arena) {
      result = cached.bindings.update(unused_frame, object);
    }
    if (result == GRANIT_SUCCESS) {
      result = cached.lighting.update_shadow({.light_view_projection = frame.light_view_projection,
                                              .texel_size = {1.0F / 1024.0F, 1.0F / 1024.0F}});
    }
    if (result != GRANIT_SUCCESS)
      break;
    granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
    if (result == GRANIT_SUCCESS)
      result = acquire_shadow_pipeline(state, material, draws[index].mesh, pipeline);
    if (result != GRANIT_SUCCESS)
      break;
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_bind_graphics_pipeline(state.renderer, recorder, pipeline);
    if (result != GRANIT_SUCCESS)
      break;
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
    if (result != GRANIT_SUCCESS)
      break;
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_viewports(state.renderer, recorder, 0, &viewport, 1);
    if (result != GRANIT_SUCCESS)
      break;
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_scissors(state.renderer, recorder, 0, &scissor, 1);
    if (result != GRANIT_SUCCESS)
      break;
    if (result == GRANIT_SUCCESS)
      result =
          granit::pipeline::detail::bind_mesh_buffers(state.renderer, recorder, draws[index].mesh);
    if (result == GRANIT_SUCCESS) {
      if (index != 0)
        depth_attachment.depth_load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD;
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
    result = trim_draw_binding_cache(state.shadow_draw_bindings, draws.size());
  return result;
}

}
