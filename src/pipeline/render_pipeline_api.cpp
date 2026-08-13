// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.h>

#include "lighting/reference_pipeline_graph.h"
#include "lighting/light_data.h"
#include "lighting/shadow_ibl_resources.h"
#include "lighting/tone_mapping_resources.h"
#include "material/material_package.h"
#include "pipeline/default_ibl_resources.h"
#include "pipeline/embedded_shaders.h"
#include "pipeline/material_access.h"
#include "pipeline/mesh_access.h"
#include "pipeline/pbr_draw_bindings.h"
#include "pipeline/scene_access.h"

#include <granit/renderer/render_target.h>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/texture.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x42);

struct pipeline_state {
  struct shadow_pipeline_entry {
    granit_pipeline_layout layout = GRANIT_NULL_HANDLE;
    granit_mesh mesh = GRANIT_NULL_HANDLE;
    granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  };

  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_render_pipeline_record_callback record = nullptr;
  void* user_data = nullptr;
  granit::lighting::tone_mapping_pipeline_resources tone_mapping_unorm;
  granit::lighting::tone_mapping_pipeline_resources tone_mapping_srgb;
  granit::pipeline::detail::default_ibl_resources default_ibl;
  granit::shader shadow_vertex_shader;
  granit::shader shadow_fragment_shader;
  granit::texture shadow_placeholder_texture;
  granit::texture_view shadow_placeholder_view;
  std::vector<shadow_pipeline_entry> shadow_pipelines;
  bool alive = true;
};

struct pipeline_slot {
  std::shared_ptr<pipeline_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<pipeline_slot> registry;

granit_handle encode(size_t index, uint32_t generation) {
  return (type_value << 56) | (static_cast<uint64_t>(generation) << 32) |
         (static_cast<uint64_t>(index) + 1);
}

bool decode(granit_handle handle, size_t& index, uint32_t& generation) {
  if ((handle >> 56) != type_value || (handle & index_mask) == 0)
    return false;
  index = static_cast<size_t>((handle & index_mask) - 1);
  generation = static_cast<uint32_t>((handle >> 32) & generation_mask);
  return generation != 0;
}

std::shared_ptr<pipeline_state> find_pipeline(granit_renderer renderer,
                                              granit_render_pipeline pipeline) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(pipeline, index, generation))
    return {};
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer) {
    return {};
  }
  return registry[index].state;
}

granit_result validate_renderer(granit_renderer renderer) {
  uint64_t size = 0;
  return granit_renderer_pipeline_cache_export(renderer, nullptr, &size);
}

granit_texture_desc make_depth_desc(uint32_t width, uint32_t height) {
  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
  desc.usage = GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  desc.width = width;
  desc.height = height;
  return desc;
}

granit_texture_desc make_shadow_desc() {
  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
  desc.usage = GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  desc.width = 1024;
  desc.height = 1024;
  return desc;
}

granit_matrix4 convert(const granit::math::matrix4& value) {
  granit_matrix4 result{};
  std::ranges::copy(value, result.elements);
  return result;
}

granit_float3 convert(granit::math::float3 value) { return {value.x, value.y, value.z}; }

granit_result record_tone_mapping(granit::lighting::tone_mapping_pipeline_resources& pipeline,
                                  granit_renderer renderer, granit_command_recorder recorder,
                                  granit_texture_view hdr_view, granit_texture_view output_view,
                                  granit_texture_format output_format, uint32_t width,
                                  uint32_t height,
                                  const granit::lighting::tone_mapping_constants& constants) {
  if (!pipeline.initialized()) {
    const auto initialize =
        pipeline.initialize(renderer, static_cast<granit::texture_format>(output_format),
                            granit::pipeline::detail::tone_mapping_vertex_shader(),
                            granit::pipeline::detail::tone_mapping_fragment_shader());
    if (initialize != GRANIT_SUCCESS)
      return initialize;
  }
  granit::lighting::tone_mapping_binding_resources binding;
  auto result = binding.initialize(pipeline, hdr_view, constants);
  if (result == GRANIT_SUCCESS) {
    result =
        granit_command_recorder_bind_graphics_pipeline(renderer, recorder, pipeline.pipeline());
  }
  const auto group = binding.group();
  if (result == GRANIT_SUCCESS) {
    result = granit_command_recorder_bind_graphics_groups(renderer, recorder,
                                                          pipeline.pipeline_layout(), 0, &group, 1);
  }
  const granit_viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const granit_scissor scissor{0, 0, width, height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_viewports(renderer, recorder, 0, &viewport, 1);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_scissors(renderer, recorder, 0, &scissor, 1);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = output_view;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area = {0, 0, width, height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_draw(renderer, recorder, 3, 1, 0, 0);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(renderer, recorder);
  const auto reset_result = binding.reset();
  return result == GRANIT_SUCCESS ? reset_result : result;
}

granit_result record_opaque_draws(
    pipeline_state& state, granit_command_recorder recorder, granit_texture_view color,
    granit_texture_view depth, granit_texture_view shadow, uint32_t width, uint32_t height,
    const granit::material::pbr_frame_constants& frame,
    std::span<const granit::material::pbr_object_constants> objects,
    std::span<const granit_render_pipeline_draw_binding> draws,
    const granit::lighting::packed_view_lights& lights,
    const granit::lighting::shadow_sampling_constants& shadow_constants) {
  if (shadow == GRANIT_NULL_HANDLE || objects.size() != draws.size())
    return GRANIT_ERROR_NOT_READY;
  granit_color_attachment_desc color_attachment = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color_attachment.view = color;
  granit_depth_stencil_attachment_desc depth_attachment =
      GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth_attachment.view = depth;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color_attachment;
  rendering.depth_stencil_attachment = &depth_attachment;
  rendering.area = {0, 0, width, height};
  auto result = granit_command_recorder_begin_rendering(state.renderer, recorder, &rendering);
  const granit_viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const granit_scissor scissor{0, 0, width, height};
  for (size_t index = 0; result == GRANIT_SUCCESS && index < draws.size(); ++index) {
    granit::pipeline::detail::material_draw_state material;
    result = granit::pipeline::detail::acquire_material_draw_state(
        state.renderer, draws[index].material,
        {.pass = granit::material::make_feature_id("opaque"),
         .variant = 0,
         .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
         .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
        material);
    granit::pipeline::detail::pbr_draw_bindings bindings;
    if (result == GRANIT_SUCCESS)
      result = bindings.initialize(state.renderer, material, frame, objects[index]);
    granit::lighting::shadow_ibl_resources lighting;
    if (result == GRANIT_SUCCESS) {
      result = lighting.initialize(
          state.renderer,
          {.shadow = shadow,
           .ibl = {.irradiance = state.default_ibl.irradiance(),
                   .prefiltered_environment = state.default_ibl.prefiltered_environment(),
                   .brdf_lut = state.default_ibl.brdf_lut()}},
          shadow_constants, {.intensity = 0.0F}, {}, {}, material.lighting_layout);
    }
    if (result == GRANIT_SUCCESS)
      result = lighting.update_lights(lights);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_bind_graphics_pipeline(state.renderer, recorder,
                                                               material.pipeline);
    const std::array groups{bindings.frame_group(), material.material_group,
                            bindings.object_group(), lighting.group()};
    if (result == GRANIT_SUCCESS) {
      result = granit_command_recorder_bind_graphics_groups(
          state.renderer, recorder, material.pipeline_layout, 0, groups.data(),
          static_cast<uint32_t>(groups.size()));
    }
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_viewports(state.renderer, recorder, 0, &viewport, 1);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_scissors(state.renderer, recorder, 0, &scissor, 1);
    if (result == GRANIT_SUCCESS) {
      result = granit::pipeline::detail::record_mesh_draw(state.renderer, recorder,
                                                          draws[index].mesh);
    }
  }
  const auto end_result = granit_command_recorder_end_rendering(state.renderer, recorder);
  return result == GRANIT_SUCCESS ? end_result : result;
}

granit_result acquire_shadow_pipeline(pipeline_state& state,
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
  auto result = granit::pipeline::detail::copy_mesh_pipeline_state(state.renderer, mesh, mesh_state);
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

granit_result record_shadow_draws(
    pipeline_state& state, granit_command_recorder recorder, granit_texture_view depth,
    const granit::lighting::shadow_frame_constants& frame,
    std::span<const granit::lighting::shadow_caster> casters,
    std::span<const granit_render_pipeline_draw_binding> draws) {
  if (casters.size() != draws.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_depth_stencil_attachment_desc depth_attachment =
      GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth_attachment.view = depth;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.depth_stencil_attachment = &depth_attachment;
  rendering.area = {0, 0, 1024, 1024};
  auto result = granit_command_recorder_begin_rendering(state.renderer, recorder, &rendering);
  const granit_viewport viewport{0, 0, 1024, 1024, 0, 1};
  const granit_scissor scissor{0, 0, 1024, 1024};
  const granit::material::pbr_frame_constants unused_frame{};
  for (size_t index = 0; result == GRANIT_SUCCESS && index < draws.size(); ++index) {
    granit::pipeline::detail::material_draw_state material;
    result = granit::pipeline::detail::acquire_material_draw_state(
        state.renderer, draws[index].material,
        {.pass = granit::material::make_feature_id("opaque"),
         .variant = 0,
         .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
         .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
        material);
    const granit::material::pbr_object_constants object{
        .model = casters[index].model,
        .normal_matrix = granit::math::identity_matrix4,
        .object_id = {casters[index].object_id, 0, 0, 0}};
    granit::pipeline::detail::pbr_draw_bindings bindings;
    if (result == GRANIT_SUCCESS)
      result = bindings.initialize(state.renderer, material, unused_frame, object);
    granit::lighting::shadow_ibl_resources lighting;
    if (result == GRANIT_SUCCESS) {
      result = lighting.initialize(
          state.renderer,
          {.shadow = state.shadow_placeholder_view.native_handle(),
           .ibl = {.irradiance = state.default_ibl.irradiance(),
                   .prefiltered_environment = state.default_ibl.prefiltered_environment(),
                   .brdf_lut = state.default_ibl.brdf_lut()}},
          {.light_view_projection = frame.light_view_projection,
           .texel_size = {1.0F / 1024.0F, 1.0F / 1024.0F}},
          {.intensity = 0.0F}, {}, {}, material.lighting_layout);
    }
    granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
    if (result == GRANIT_SUCCESS)
      result = acquire_shadow_pipeline(state, material, draws[index].mesh, pipeline);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_bind_graphics_pipeline(state.renderer, recorder, pipeline);
    const std::array groups{bindings.object_group(), lighting.group()};
    if (result == GRANIT_SUCCESS) {
      result = granit_command_recorder_bind_graphics_groups(
          state.renderer, recorder, material.pipeline_layout, 2, groups.data(),
          static_cast<uint32_t>(groups.size()));
    }
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_viewports(state.renderer, recorder, 0, &viewport, 1);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_scissors(state.renderer, recorder, 0, &scissor, 1);
    if (result == GRANIT_SUCCESS)
      result = granit::pipeline::detail::record_mesh_draw(state.renderer, recorder,
                                                          draws[index].mesh);
  }
  const auto end_result = granit_command_recorder_end_rendering(state.renderer, recorder);
  return result == GRANIT_SUCCESS ? end_result : result;
}

granit_result
render_view(pipeline_state& state, const granit_render_pipeline_render_desc& desc,
            const granit::scene::multi_view_snapshot& snapshot,
            const std::unordered_map<uint64_t, granit_render_pipeline_draw_binding>& bindings,
            uint32_t view_index) {
  const auto& visible = snapshot.views()[view_index];
  if (visible.renderables.indices().empty())
    return GRANIT_ERROR_NOT_READY;

  granit::render_graph::serial_graph graph;
  const auto hdr = graph.create_transient_texture(
      granit::lighting::make_hdr_attachment_desc(desc.width, desc.height), "Reference HDR");
  const auto depth =
      graph.create_transient_texture(make_depth_desc(desc.width, desc.height), "Reference Depth");
  const auto output = graph.import_texture_view(desc.output, true, "Reference Output");

  std::optional<granit::render_graph::resource_id> shadow;
  if (!visible.directional_lights.empty())
    shadow = graph.create_transient_texture(make_shadow_desc(), "Reference Directional Shadow");

  granit::lighting::reference_pipeline_graph_desc graph_desc;
  graph_desc.pbr.color = hdr;
  graph_desc.pbr.depth = depth;
  graph_desc.pbr.view.view_projection = visible.view.view_projection;
  graph_desc.pbr.view.camera_position = visible.view.camera_position;
  granit::lighting::shadow_sampling_constants shadow_constants{};
  if (!visible.directional_lights.empty()) {
    const auto& light = snapshot.directional_lights()[visible.directional_lights.front()];
    graph_desc.pbr.light.direction_to_light = light.direction_to_light;
    graph_desc.pbr.light.radiance = light.radiance;
    granit::lighting::directional_shadow_pass_desc shadow_pass;
    const granit::lighting::directional_shadow_volume volume{.focus = visible.view.camera_position,
                                                             .half_width = 20.0F,
                                                             .half_height = 20.0F,
                                                             .near_plane = 0.1F,
                                                             .far_plane = 100.0F,
                                                             .light_distance = 50.0F};
    const auto shadow_result = granit::lighting::build_directional_shadow_pass_desc(
        snapshot, view_index, visible.directional_lights.front(), volume, *shadow, shadow_pass);
    if (shadow_result != granit::lighting::directional_shadow_error::none &&
        shadow_result != granit::lighting::directional_shadow_error::no_casters) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    if (shadow_result == granit::lighting::directional_shadow_error::none) {
      shadow_constants = {.light_view_projection = shadow_pass.frame.light_view_projection,
                          .depth_bias = 0.001F,
                          .normal_bias = 0.0F,
                          .texel_size = {1.0F / 1024.0F, 1.0F / 1024.0F}};
      graph_desc.pbr.shadow = *shadow;
      graph_desc.shadow = std::move(shadow_pass);
    } else {
      shadow.reset();
    }
  }
  std::vector<uint64_t> payloads;
  std::vector<granit_render_pipeline_draw_binding> draw_bindings;
  std::vector<granit_scene_renderable> renderables;
  payloads.reserve(visible.renderables.indices().size());
  draw_bindings.reserve(visible.renderables.indices().size());
  renderables.reserve(visible.renderables.indices().size());
  graph_desc.pbr.objects.reserve(visible.renderables.indices().size());
  for (const auto index : visible.renderables.indices()) {
    const auto& source = snapshot.renderables()[index];
    graph_desc.pbr.objects.push_back({.model = source.model,
                                      .normal_matrix = source.normal_matrix,
                                      .object_id = source.object_id});
    payloads.push_back(source.payload);
    const auto binding = bindings.find(source.payload);
    if (binding == bindings.end())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    draw_bindings.push_back(binding->second);
    renderables.push_back({.model = convert(source.model),
                           .normal_matrix = convert(source.normal_matrix),
                           .bounds_center = convert(source.bounds.center),
                           .bounds_radius = source.bounds.radius,
                           .layer_mask = source.layer_mask,
                           .sort_key = source.sort_key,
                           .payload = source.payload,
                           .object_id = source.object_id,
                           .reserved = 0});
  }
  const granit_scene_view public_view{.view = convert(visible.view.view),
                                      .projection = convert(visible.view.projection),
                                      .view_projection = convert(visible.view.view_projection),
                                      .camera_position = convert(visible.view.camera_position),
                                      .viewport_x = visible.view.area.x,
                                      .viewport_y = visible.view.area.y,
                                      .viewport_width = visible.view.area.width,
                                      .viewport_height = visible.view.area.height,
                                      .layer_mask = visible.view.layer_mask};
  graph_desc.tone_mapping.hdr_color = hdr;
  graph_desc.tone_mapping.output = output;
  graph_desc.tone_mapping.output_format = static_cast<granit::texture_format>(desc.output_format);
  graph_desc.tone_mapping.tone_mapping.exposure_ev = desc.exposure_ev;
  graph_desc.tone_mapping.tone_mapping.output_transfer =
      desc.output_format == GRANIT_TEXTURE_FORMAT_RGBA8_SRGB
          ? granit::lighting::tone_mapping_output_transfer::attachment_srgb
          : granit::lighting::tone_mapping_output_transfer::shader_srgb;

  granit::lighting::reference_pipeline_graph_callbacks callbacks;
  callbacks.pbr = [&](auto& context, const auto& frame, auto objects) {
    if (state.record == nullptr) {
      granit::lighting::packed_view_lights lights;
      granit::lighting::light_requirements requirements;
      if (granit::lighting::pack_view_lights(snapshot, view_index, {}, lights, requirements) !=
          granit::lighting::light_pack_error::none) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      return record_opaque_draws(state, context.recorder(), context.texture_view(hdr),
                                 context.texture_view(depth),
                                 shadow ? context.texture_view(*shadow) : GRANIT_NULL_HANDLE,
                                 desc.width, desc.height, frame, objects, draw_bindings, lights,
                                 shadow_constants);
    }
    const granit_render_pipeline_record_info info{
        .struct_size = sizeof(granit_render_pipeline_record_info),
        .stage = GRANIT_RENDER_PIPELINE_STAGE_OPAQUE,
        .recorder = context.recorder(),
        .color_input = GRANIT_NULL_HANDLE,
        .color_output = context.texture_view(hdr),
        .depth_output = context.texture_view(depth),
        .shadow_input = shadow ? context.texture_view(*shadow) : GRANIT_NULL_HANDLE,
        .ibl_irradiance = state.default_ibl.irradiance(),
        .ibl_prefiltered_environment = state.default_ibl.prefiltered_environment(),
        .ibl_brdf_lut = state.default_ibl.brdf_lut(),
        .ibl_layout = state.default_ibl.layout(),
        .ibl_group = state.default_ibl.group(),
        .view_index = view_index,
        .payload_count = static_cast<uint32_t>(payloads.size()),
        .payloads = payloads.data(),
        .draw_bindings = draw_bindings.data(),
        .view = &public_view,
        .renderables = renderables.data(),
        .light_view_projection = {},
        .exposure_scale = 1.0F,
        .encode_srgb = 0,
        .reserved = {0, 0}};
    return state.record(&info, state.user_data);
  };
  if (graph_desc.shadow) {
    callbacks.shadow = [&](auto& context, const auto& frame, auto casters) {
      std::vector<uint64_t> shadow_payloads;
      std::vector<granit_render_pipeline_draw_binding> shadow_bindings;
      std::vector<granit_scene_renderable> shadow_renderables;
      shadow_payloads.reserve(casters.size());
      shadow_bindings.reserve(casters.size());
      shadow_renderables.reserve(casters.size());
      for (const auto& caster : casters) {
        const auto binding = bindings.find(caster.payload);
        if (binding == bindings.end())
          return GRANIT_ERROR_INVALID_ARGUMENT;
        const auto& source = snapshot.renderables()[caster.source_index];
        shadow_payloads.push_back(caster.payload);
        shadow_bindings.push_back(binding->second);
        shadow_renderables.push_back({.model = convert(source.model),
                                      .normal_matrix = convert(source.normal_matrix),
                                      .bounds_center = convert(source.bounds.center),
                                      .bounds_radius = source.bounds.radius,
                                      .layer_mask = source.layer_mask,
                                      .sort_key = source.sort_key,
                                      .payload = source.payload,
                                      .object_id = source.object_id,
                                      .reserved = 0});
      }
      if (state.record == nullptr) {
        return record_shadow_draws(state, context.recorder(), context.texture_view(*shadow), frame,
                                   casters, shadow_bindings);
      }
      const granit_render_pipeline_record_info info{
          .struct_size = sizeof(granit_render_pipeline_record_info),
          .stage = GRANIT_RENDER_PIPELINE_STAGE_SHADOW,
          .recorder = context.recorder(),
          .color_input = GRANIT_NULL_HANDLE,
          .color_output = GRANIT_NULL_HANDLE,
          .depth_output = context.texture_view(*shadow),
          .shadow_input = GRANIT_NULL_HANDLE,
          .ibl_irradiance = GRANIT_NULL_HANDLE,
          .ibl_prefiltered_environment = GRANIT_NULL_HANDLE,
          .ibl_brdf_lut = GRANIT_NULL_HANDLE,
          .ibl_layout = GRANIT_NULL_HANDLE,
          .ibl_group = GRANIT_NULL_HANDLE,
          .view_index = view_index,
          .payload_count = static_cast<uint32_t>(shadow_payloads.size()),
          .payloads = shadow_payloads.data(),
          .draw_bindings = shadow_bindings.data(),
          .view = &public_view,
          .renderables = shadow_renderables.data(),
          .light_view_projection = convert(frame.light_view_projection),
          .exposure_scale = 1.0F,
          .encode_srgb = 0,
          .reserved = {0, 0}};
      return state.record(&info, state.user_data);
    };
  }
  callbacks.tone_mapping = [&](auto& context, const auto& constants) {
    auto& pipeline = desc.output_format == GRANIT_TEXTURE_FORMAT_RGBA8_SRGB
                         ? state.tone_mapping_srgb
                         : state.tone_mapping_unorm;
    return record_tone_mapping(pipeline, state.renderer, context.recorder(),
                               context.texture_view(hdr), context.texture_view(output),
                               desc.output_format, desc.width, desc.height, constants);
  };
  granit::lighting::reference_pipeline_graph_passes passes;
  if (granit::lighting::add_reference_pipeline_graph(graph, std::move(graph_desc),
                                                     std::move(callbacks), passes) !=
      granit::lighting::reference_pipeline_graph_error::none) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto execution = graph.execute(state.renderer);
  if (!execution.succeeded())
    return execution.result;
  return granit_command_recorder_destroy(state.renderer, execution.recorder);
}

} // namespace

extern "C" granit_result granit_render_pipeline_create(granit_renderer renderer,
                                                       const granit_render_pipeline_desc* desc,
                                                       granit_render_pipeline* pipeline) {
  if (pipeline == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *pipeline = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < sizeof(granit_render_pipeline_desc) ||
      desc->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto renderer_result = validate_renderer(renderer);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;
  try {
    auto state = std::make_shared<pipeline_state>();
    state->renderer = renderer;
    state->record = desc->record;
    state->user_data = desc->user_data;
    const auto ibl_result = state->default_ibl.initialize(renderer);
    if (ibl_result != GRANIT_SUCCESS)
      return ibl_result;
    auto resource_result = state->shadow_vertex_shader.initialize(
        renderer, {.stage = granit::shader_stage::vertex,
                   .code = granit::pipeline::detail::shadow_depth_vertex_shader(),
                   .entry_point = "vertex_main"});
    if (granit::failed(resource_result))
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_fragment_shader.initialize(
        renderer, {.stage = granit::shader_stage::fragment,
                   .code = granit::pipeline::detail::shadow_depth_fragment_shader(),
                   .entry_point = "fragment_main"});
    if (granit::failed(resource_result))
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_placeholder_texture.initialize(
        renderer, {.format = granit::texture_format::d32_float,
                   .usage = granit::texture_usage::sampled |
                            granit::texture_usage::depth_stencil_attachment});
    if (granit::failed(resource_result))
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_placeholder_view.initialize(
        renderer, state->shadow_placeholder_texture.native_handle());
    if (granit::failed(resource_result))
      return static_cast<granit_result>(resource_result);
    std::scoped_lock lock{registry_mutex};
    size_t index = 0;
    while (index < registry.size() && registry[index].state != nullptr)
      ++index;
    if (index == registry.size())
      registry.emplace_back();
    registry[index].state = std::move(state);
    *pipeline = encode(index, registry[index].generation);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_render_pipeline_render(granit_renderer renderer, granit_render_pipeline pipeline,
                              const granit_render_pipeline_render_desc* desc) {
  if (desc == nullptr || desc->struct_size < sizeof(granit_render_pipeline_render_desc) ||
      desc->reserved != 0 || desc->reserved_tail != 0 || desc->scene == GRANIT_NULL_HANDLE ||
      desc->output == GRANIT_NULL_HANDLE || desc->width == 0 || desc->height == 0 ||
      desc->view_count == 0 || !std::isfinite(desc->exposure_ev) ||
      (desc->draw_binding_count != 0 && desc->draw_bindings == nullptr)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto state = find_pipeline(renderer, pipeline);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    std::unique_lock lock{state->mutex, std::try_to_lock};
    if (!lock.owns_lock())
      return GRANIT_ERROR_NOT_READY;
    if (!state->alive)
      return GRANIT_ERROR_INVALID_HANDLE;
    std::unordered_map<uint64_t, granit_render_pipeline_draw_binding> bindings;
    bindings.reserve(desc->draw_binding_count);
    for (uint32_t index = 0; index < desc->draw_binding_count; ++index) {
      const auto& binding = desc->draw_bindings[index];
      if (binding.mesh == 0 || binding.material == GRANIT_NULL_HANDLE || binding.reserved != 0 ||
          !bindings.emplace(binding.payload, binding).second) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      const auto material_result =
          granit::pipeline::detail::validate_material_handle(renderer, binding.material);
      if (material_result != GRANIT_SUCCESS)
        return material_result;
      const auto mesh_result =
          granit::pipeline::detail::validate_mesh_handle(renderer, binding.mesh);
      if (mesh_result != GRANIT_SUCCESS)
        return mesh_result;
    }
    granit::scene::multi_view_snapshot snapshot;
    auto result = granit::pipeline::detail::copy_scene_snapshot(renderer, desc->scene, snapshot);
    if (result != GRANIT_SUCCESS)
      return result;
    if (desc->first_view >= snapshot.views().size() ||
        desc->view_count > snapshot.views().size() - desc->first_view) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    for (uint32_t offset = 0; offset < desc->view_count; ++offset) {
      result = render_view(*state, *desc, snapshot, bindings, desc->first_view + offset);
      if (result != GRANIT_SUCCESS)
        return result;
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_render_pipeline_destroy(granit_renderer renderer,
                                                        granit_render_pipeline pipeline) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(pipeline, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<pipeline_state> removed;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].generation != generation ||
        registry[index].state == nullptr || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    removed = std::move(registry[index].state);
    registry[index].generation =
        registry[index].generation == generation_mask ? 1 : registry[index].generation + 1;
  }
  std::scoped_lock lock{removed->mutex};
  removed->alive = false;
  auto result = removed->tone_mapping_unorm.reset();
  const auto srgb_result = removed->tone_mapping_srgb.reset();
  if (result == GRANIT_SUCCESS)
    result = srgb_result;
  const auto ibl_result = removed->default_ibl.reset();
  if (result == GRANIT_SUCCESS)
    result = ibl_result;
  for (const auto& entry : removed->shadow_pipelines) {
    const auto pipeline_result =
        granit_graphics_pipeline_destroy(renderer, entry.pipeline);
    if (result == GRANIT_SUCCESS)
      result = pipeline_result;
  }
  removed->shadow_pipelines.clear();
  const auto placeholder_view_result = removed->shadow_placeholder_view.reset();
  if (result == GRANIT_SUCCESS)
    result = static_cast<granit_result>(placeholder_view_result);
  const auto placeholder_texture_result = removed->shadow_placeholder_texture.reset();
  if (result == GRANIT_SUCCESS)
    result = static_cast<granit_result>(placeholder_texture_result);
  const auto fragment_result = removed->shadow_fragment_shader.reset();
  if (result == GRANIT_SUCCESS)
    result = static_cast<granit_result>(fragment_result);
  const auto vertex_result = removed->shadow_vertex_shader.reset();
  if (result == GRANIT_SUCCESS)
    result = static_cast<granit_result>(vertex_result);
  return result;
}
