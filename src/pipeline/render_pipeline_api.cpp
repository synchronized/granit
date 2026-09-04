// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.h>

#include "lighting/forward_pipeline_graph.h"
#include "lighting/light_data.h"
#include "lighting/shadow_ibl_resources.h"
#include "lighting/tone_mapping_resources.h"
#include "material/material_package.h"
#include "pipeline/default_ibl_resources.h"
#include "pipeline/dynamic_uniform_arena.h"
#include "pipeline/embedded_shaders.h"
#include "pipeline/forward_draw_recorder.h"
#include "pipeline/lighting_submission.h"
#include "pipeline/material_access.h"
#include "pipeline/mesh_access.h"
#include "pipeline/pbr_draw_bindings.h"
#include "pipeline/render_pipeline_metrics.h"
#include "pipeline/render_pipeline_state.h"
#include "pipeline/render_view_submission.h"
#include "pipeline/scene_access.h"
#include "pipeline/shadow_draw_recorder.h"
#include "pipeline/tone_mapping_recorder.h"

#include <granit/renderer/frame_context.h>
#include <granit/renderer/render_target.h>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/timestamp_query.h>

#include <algorithm>
#include <array>
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
// 自动路径固定覆盖首轮多光源评估上限，打包和逐 Draw Buffer 必须使用相同容量。
constexpr granit::lighting::light_limits automatic_light_limits{
    .directional = 4, .point = 128, .spot = 64};

using pipeline_state = granit::pipeline::detail::render_pipeline_state;

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
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(renderer, &status);
}

granit_texture_desc make_depth_desc(uint32_t width, uint32_t height,
                                    granit_sample_count samples = GRANIT_SAMPLE_COUNT_1) {
  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
  desc.usage = GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  desc.width = width;
  desc.height = height;
  desc.sample_count = samples;
  return desc;
}

granit_matrix4 convert(const granit::math::matrix4& value) {
  granit_matrix4 result{};
  std::ranges::copy(value, result.elements);
  return result;
}

granit_float3 convert(granit::math::float3 value) { return {value.x, value.y, value.z}; }

bool is_srgb_output(granit_texture_format format) {
  return format == GRANIT_TEXTURE_FORMAT_RGBA8_SRGB || format == GRANIT_TEXTURE_FORMAT_BGRA8_SRGB;
}

size_t tone_mapping_pipeline_index(granit_texture_format format) {
  return static_cast<size_t>(format - GRANIT_TEXTURE_FORMAT_RGBA8_UNORM);
}

bool valid_output(const granit_render_pipeline_output& output) {
  return output.struct_size >= GRANIT_RENDER_PIPELINE_OUTPUT_VERSION_1_SIZE &&
         output.reserved == 0 && output.reserved_tail == 0 && output.view != GRANIT_NULL_HANDLE &&
         output.width != 0 && output.height != 0 &&
         output.format >= GRANIT_TEXTURE_FORMAT_RGBA8_UNORM &&
         output.format <= GRANIT_TEXTURE_FORMAT_BGRA8_SRGB;
}

granit_result
render_view(pipeline_state& state, const granit_render_pipeline_render_desc& desc,
            const granit::scene::multi_view_snapshot& snapshot,
            const std::unordered_map<uint64_t, granit_render_pipeline_draw_binding>& bindings,
            uint32_t view_index, const granit_render_pipeline_output& render_output,
            granit_frame frame) {
  const auto& visible = snapshot.views()[view_index];
  granit::pipeline::detail::render_view_submission view_submission;
  auto result = granit::pipeline::detail::build_render_view_submission(snapshot, view_index,
                                                                       bindings, view_submission);
  if (result != GRANIT_SUCCESS)
    return result;
  granit::pipeline::detail::lighting_submission lighting_submission;
  if (state.record == nullptr) {
    result = granit::pipeline::detail::build_lighting_submission(
        snapshot, view_index, automatic_light_limits,
        {.irradiance = state.default_ibl.irradiance(),
         .prefiltered_environment = state.default_ibl.prefiltered_environment(),
         .brdf_lut = state.default_ibl.brdf_lut()},
        desc.environment, lighting_submission);
    if (result != GRANIT_SUCCESS)
      return result;
  }

  granit_frame_info frame_info = GRANIT_FRAME_INFO_INIT;
  const bool use_uniform_arena = frame != GRANIT_NULL_HANDLE;
  if (use_uniform_arena) {
    const auto slot_result = granit_frame_get_slot_info(state.renderer, frame, &frame_info);
    if (slot_result != GRANIT_SUCCESS)
      return slot_result;
    const auto arena_result =
        state.uniform_arena.begin_frame(frame_info.frame_slot, frame_info.frame_slot_count);
    if (arena_result != GRANIT_SUCCESS)
      return arena_result;
  }
  const auto metrics_slot_index = use_uniform_arena ? frame_info.frame_slot : 0U;
  const auto metrics_slot_count = use_uniform_arena ? frame_info.frame_slot_count : 1U;
  const auto metrics_pool = granit::pipeline::detail::prepare_render_pipeline_metrics_slot(
      state, metrics_slot_index, metrics_slot_count);

  granit::render_graph::serial_graph graph;
  const bool use_msaa = state.record == nullptr && state.sample_count == GRANIT_SAMPLE_COUNT_4;
  const auto hdr = graph.create_transient_texture(
      granit::lighting::make_hdr_attachment_desc(render_output.width, render_output.height),
      "Reference HDR");
  auto msaa_color_desc =
      granit::lighting::make_hdr_attachment_desc(render_output.width, render_output.height);
  msaa_color_desc.sample_count = GRANIT_SAMPLE_COUNT_4;
  msaa_color_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  const auto msaa_color =
      use_msaa ? graph.create_transient_texture(msaa_color_desc, "Reference HDR MSAA")
               : granit::render_graph::invalid_resource_id;
  const auto depth = graph.create_transient_texture(
      make_depth_desc(render_output.width, render_output.height,
                      use_msaa ? GRANIT_SAMPLE_COUNT_4 : GRANIT_SAMPLE_COUNT_1),
      "Reference Depth");
  const auto output = graph.import_texture_view(render_output.view, true, "Reference Output");

  std::optional<granit::render_graph::resource_id> shadow = graph.import_texture_view(
      state.shadow_view.native_handle(), false, "Reference Directional Shadow");

  granit::lighting::forward_pipeline_graph_desc graph_desc;
  graph_desc.pbr.color = use_msaa ? msaa_color : hdr;
  graph_desc.pbr.resolve_color = use_msaa ? hdr : granit::render_graph::invalid_resource_id;
  graph_desc.pbr.depth = depth;
  graph_desc.pbr.shadow = *shadow;
  graph_desc.pbr.view.view_projection = visible.view.view_projection;
  graph_desc.pbr.view.camera_position = visible.view.camera_position;
  graph_desc.pbr.objects = view_submission.pbr_objects;
  granit::lighting::shadow_sampling_constants shadow_constants{
      .light_view_projection = granit::math::identity_matrix4, .texel_size = {1.0F, 1.0F}};
  if (!visible.directional_lights.empty()) {
    const auto& light = snapshot.directional_lights()[visible.directional_lights.front()];
    graph_desc.pbr.light.direction_to_light = light.direction_to_light;
    graph_desc.pbr.light.radiance = light.radiance;
    granit::lighting::directional_shadow_pass_desc shadow_pass;
    const granit::lighting::directional_shadow_volume volume{.focus = visible.view.camera_position,
                                                             .half_width = state.shadow_half_extent,
                                                             .half_height =
                                                                 state.shadow_half_extent,
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
  graph_desc.tone_mapping.hdr_color = hdr;
  graph_desc.tone_mapping.output = output;
  graph_desc.tone_mapping.output_format = static_cast<granit::texture_format>(render_output.format);
  graph_desc.tone_mapping.tone_mapping.exposure_ev = desc.exposure_ev;
  graph_desc.tone_mapping.tone_mapping.enable_fxaa = state.enable_fxaa;
  graph_desc.tone_mapping.tone_mapping.output_transfer =
      is_srgb_output(render_output.format)
          ? granit::lighting::tone_mapping_output_transfer::attachment_srgb
          : granit::lighting::tone_mapping_output_transfer::shader_srgb;

  bool metrics_reset = false;
  const auto measure = [&](granit_command_recorder recorder, uint32_t first, auto&& operation) {
    if (metrics_pool == GRANIT_NULL_HANDLE)
      return operation();
    auto result = GRANIT_SUCCESS;
    if (!metrics_reset) {
      result = granit_command_recorder_reset_timestamp_queries(state.renderer, recorder,
                                                               metrics_pool, 0, 8);
      metrics_reset = result == GRANIT_SUCCESS;
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_write_timestamp(state.renderer, recorder, metrics_pool,
                                                         GRANIT_TIMESTAMP_STAGE_TOP, 6);
    }
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_write_timestamp(state.renderer, recorder, metrics_pool,
                                                       GRANIT_TIMESTAMP_STAGE_TOP, first);
    if (result == GRANIT_SUCCESS)
      result = operation();
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_write_timestamp(state.renderer, recorder, metrics_pool,
                                                       GRANIT_TIMESTAMP_STAGE_BOTTOM, first + 1);
    return result;
  };
  granit::lighting::forward_pipeline_graph_callbacks callbacks;
  callbacks.pbr = [&](auto& context, const auto& frame, auto objects) {
    return measure(context.recorder(), 2, [&]() {
      if (state.record == nullptr) {
        auto configured_frame = frame;
        configured_frame.render_options[0] = state.enable_specular_aa ? UINT32_C(1) : UINT32_C(0);
        const auto opaque_result = granit::pipeline::detail::record_opaque_draws(
            state, context.recorder(), context.texture_view(use_msaa ? msaa_color : hdr),
            use_msaa ? context.texture_view(hdr) : GRANIT_NULL_HANDLE, context.texture_view(depth),
            shadow ? context.texture_view(*shadow) : GRANIT_NULL_HANDLE, render_output.width,
            render_output.height, configured_frame, objects, view_submission.draw_bindings,
            lighting_submission.lights, shadow_constants, lighting_submission.ibl_views,
            lighting_submission.ibl_constants(), use_uniform_arena, desc.clear_color);
        return opaque_result;
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
          .payload_count = static_cast<uint32_t>(view_submission.payloads.size()),
          .payloads = view_submission.payloads.data(),
          .draw_bindings = view_submission.draw_bindings.data(),
          .view = &view_submission.view,
          .renderables = view_submission.renderables.data(),
          .light_view_projection = {},
          .exposure_scale = 1.0F,
          .encode_srgb = 0,
          .reserved = {0, 0}};
      return state.record(&info, state.user_data);
    });
  };
  if (graph_desc.shadow) {
    callbacks.shadow = [&](auto& context, const auto& frame, auto casters) {
      return measure(context.recorder(), 0, [&]() {
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
          const auto shadow_result = granit::pipeline::detail::record_shadow_draws(
              state, context.recorder(), context.texture_view(*shadow), frame, casters,
              shadow_bindings, use_uniform_arena);
          return shadow_result;
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
            .view = &view_submission.view,
            .renderables = shadow_renderables.data(),
            .light_view_projection = convert(frame.light_view_projection),
            .exposure_scale = 1.0F,
            .encode_srgb = 0,
            .reserved = {0, 0}};
        return state.record(&info, state.user_data);
      });
    };
  }
  callbacks.tone_mapping = [&](auto& context, const auto& constants) {
    const auto result = measure(context.recorder(), 4, [&]() {
      auto& pipeline =
          state.tone_mapping_pipelines[tone_mapping_pipeline_index(render_output.format)];
      const auto tone_result = granit::pipeline::detail::record_tone_mapping(
          pipeline, state.renderer, context.recorder(), context.texture_view(hdr),
          context.texture_view(output), render_output.format, render_output.width,
          render_output.height, constants);
      return tone_result;
    });
    if (result != GRANIT_SUCCESS || metrics_pool == GRANIT_NULL_HANDLE)
      return result;
    return granit_command_recorder_write_timestamp(state.renderer, context.recorder(), metrics_pool,
                                                   GRANIT_TIMESTAMP_STAGE_BOTTOM, 7);
  };
  granit::lighting::forward_pipeline_graph_passes passes;
  if (granit::lighting::add_forward_pipeline_graph(graph, std::move(graph_desc),
                                                   std::move(callbacks), passes) !=
      granit::lighting::forward_pipeline_graph_error::none) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto overlay_dependency = passes.tone_mapping;
  if (render_output.debug_draw != GRANIT_NULL_HANDLE) {
    const auto debug_pass = graph.add_pass(
        {.side_effect = true,
         .accesses = {{output, granit::render_graph::access_type::read_write},
                      {depth, granit::render_graph::access_type::read}}},
        [&](granit::render_graph::pass_context& context) {
          granit_debug_draw_record_desc debug_desc = GRANIT_DEBUG_DRAW_RECORD_DESC_INIT;
          debug_desc.color = context.texture_view(output);
          debug_desc.color_format = render_output.format;
          debug_desc.depth = context.texture_view(depth);
          debug_desc.depth_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
          debug_desc.width = render_output.width;
          debug_desc.height = render_output.height;
          debug_desc.view_projection = view_submission.view.view_projection;
          debug_desc.encode_srgb = is_srgb_output(render_output.format) ? 0U : 1U;
          return granit_debug_draw_list_record_world(state.renderer, context.recorder(),
                                                     render_output.debug_draw, &debug_desc);
        },
        "Reference / World Debug Draw");
    if (debug_pass == granit::render_graph::invalid_pass_id ||
        !graph.add_dependency(passes.tone_mapping, debug_pass)) {
      return GRANIT_ERROR_INTERNAL;
    }
    overlay_dependency = debug_pass;
  }
  if (state.record != nullptr || render_output.canvas != GRANIT_NULL_HANDLE) {
    const auto canvas_pass = graph.add_pass(
        {.side_effect = true,
         .accesses = {{output, granit::render_graph::access_type::read_write}}},
        [&](granit::render_graph::pass_context& context) {
          if (render_output.canvas != GRANIT_NULL_HANDLE) {
            granit_canvas_record_desc canvas_desc = GRANIT_CANVAS_RECORD_DESC_INIT;
            canvas_desc.color = context.texture_view(output);
            canvas_desc.color_format = render_output.format;
            canvas_desc.width = render_output.width;
            canvas_desc.height = render_output.height;
            canvas_desc.encode_srgb = is_srgb_output(render_output.format) ? 0U : 1U;
            const auto canvas_result = granit_canvas_draw_list_record(
                state.renderer, context.recorder(), render_output.canvas, &canvas_desc);
            if (canvas_result != GRANIT_SUCCESS)
              return canvas_result;
          }
          if (state.record == nullptr)
            return GRANIT_SUCCESS;
          const granit_render_pipeline_record_info info{
              .struct_size = sizeof(granit_render_pipeline_record_info),
              .stage = GRANIT_RENDER_PIPELINE_STAGE_OVERLAY,
              .recorder = context.recorder(),
              .color_input = context.texture_view(output),
              .color_output = context.texture_view(output),
              .depth_output = GRANIT_NULL_HANDLE,
              .shadow_input = GRANIT_NULL_HANDLE,
              .ibl_irradiance = GRANIT_NULL_HANDLE,
              .ibl_prefiltered_environment = GRANIT_NULL_HANDLE,
              .ibl_brdf_lut = GRANIT_NULL_HANDLE,
              .ibl_layout = GRANIT_NULL_HANDLE,
              .ibl_group = GRANIT_NULL_HANDLE,
              .view_index = view_index,
              .payload_count = 0,
              .payloads = nullptr,
              .draw_bindings = nullptr,
              .view = &view_submission.view,
              .renderables = nullptr,
              .light_view_projection = {},
              .exposure_scale = 1.0F,
              .encode_srgb = is_srgb_output(render_output.format) ? 0U : 1U,
              .reserved = {0, 0}};
          return state.record(&info, state.user_data);
        },
        "Reference / Overlay");
    if (canvas_pass == granit::render_graph::invalid_pass_id ||
        !graph.add_dependency(overlay_dependency, canvas_pass)) {
      return GRANIT_ERROR_INTERNAL;
    }
  }
  granit::render_graph::execution_result execution;
  if (frame == GRANIT_NULL_HANDLE) {
    execution = graph.execute(state.renderer);
  } else {
    execution = graph.execute_frame(state.renderer, frame, frame_info.frame_slot);
  }
  if (!execution.succeeded())
    return execution.result;
  const auto destroy_result = granit_command_recorder_destroy(state.renderer, execution.recorder);
  if (destroy_result != GRANIT_SUCCESS || metrics_pool == GRANIT_NULL_HANDLE)
    return destroy_result;
  auto& slot = state.metrics_slots[metrics_slot_index];
  slot.pending = true;
  if (!use_uniform_arena)
    static_cast<void>(granit::pipeline::detail::publish_render_pipeline_metrics(state, slot));
  // 可选指标回读不能改变已经成功提交的渲染结果。
  return GRANIT_SUCCESS;
}

} // namespace

extern "C" granit_result granit_render_pipeline_create(granit_renderer renderer,
                                                       const granit_render_pipeline_desc* desc,
                                                       granit_render_pipeline* pipeline) {
  if (pipeline == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *pipeline = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_RENDER_PIPELINE_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->enable_fxaa > 1 || desc->enable_specular_aa > 1) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if ((desc->sample_count != GRANIT_SAMPLE_COUNT_1 &&
       desc->sample_count != GRANIT_SAMPLE_COUNT_4) ||
      desc->reserved_2 != 0) {
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
    state->sample_count = desc->sample_count;
    state->enable_fxaa = desc->enable_fxaa != 0;
    state->enable_specular_aa = desc->enable_specular_aa != 0;
    const auto arena_result = state->uniform_arena.initialize(renderer);
    if (arena_result != GRANIT_SUCCESS)
      return arena_result;
    const auto ibl_result = state->default_ibl.initialize(renderer);
    if (ibl_result != GRANIT_SUCCESS)
      return ibl_result;
    auto resource_result = state->shadow_texture.initialize(
        renderer,
        {.format = granit::texture_format::d32_float,
         .usage = granit::texture_usage::depth_stencil_attachment | granit::texture_usage::sampled,
         .width = 1024,
         .height = 1024});
    if (resource_result.failed())
      return static_cast<granit_result>(resource_result);
    resource_result =
        state->shadow_view.initialize(renderer, state->shadow_texture.native_handle());
    if (resource_result.failed())
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_vertex_shader.initialize_asset(
        renderer, {.stage = granit::shader_stage::vertex,
                   .spirv = granit::pipeline::detail::shadow_depth_vertex_shader(),
                   .wgsl = granit::pipeline::detail::shadow_depth_vertex_wgsl(),
                   .entry_point = "vertex_main"});
    if (resource_result.failed())
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_fragment_shader.initialize_asset(
        renderer, {.stage = granit::shader_stage::fragment,
                   .spirv = granit::pipeline::detail::shadow_depth_fragment_shader(),
                   .wgsl = granit::pipeline::detail::shadow_depth_fragment_wgsl(),
                   .entry_point = "fragment_main"});
    if (resource_result.failed())
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_placeholder_texture.initialize(
        renderer, {.format = granit::texture_format::d32_float,
                   .usage = granit::texture_usage::sampled |
                            granit::texture_usage::depth_stencil_attachment});
    if (resource_result.failed())
      return static_cast<granit_result>(resource_result);
    resource_result = state->shadow_placeholder_view.initialize(
        renderer, state->shadow_placeholder_texture.native_handle());
    if (resource_result.failed())
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
  const auto valid_clear_color = [](const granit_clear_color_value& color) {
    return std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue) &&
           std::isfinite(color.alpha);
  };
  const auto valid_environment = [](const granit_render_pipeline_environment* environment) {
    if (environment == nullptr)
      return true;
    return environment->struct_size >= GRANIT_RENDER_PIPELINE_ENVIRONMENT_VERSION_1_SIZE &&
           environment->reserved == 0 && environment->reserved_tail == 0 &&
           environment->irradiance != GRANIT_NULL_HANDLE &&
           environment->prefiltered_environment != GRANIT_NULL_HANDLE &&
           environment->brdf_lut != GRANIT_NULL_HANDLE &&
           std::isfinite(environment->rotation_radians) && std::isfinite(environment->intensity) &&
           environment->intensity >= 0.0F && std::isfinite(environment->prefiltered_max_mip) &&
           environment->prefiltered_max_mip >= 0.0F;
  };
  if (desc == nullptr || desc->struct_size < GRANIT_RENDER_PIPELINE_RENDER_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->reserved_tail != 0 || desc->scene == GRANIT_NULL_HANDLE ||
      desc->view_count == 0 || !std::isfinite(desc->exposure_ev) ||
      (desc->frame != GRANIT_NULL_HANDLE && desc->view_count != 1) ||
      (desc->output_count == 0 && desc->view_count != 1) ||
      (desc->output_count != 0 &&
       (desc->output_count != desc->view_count || desc->outputs == nullptr)) ||
      (desc->draw_binding_count != 0 && desc->draw_bindings == nullptr) ||
      !valid_clear_color(desc->clear_color) || !valid_environment(desc->environment)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const granit_render_pipeline_output legacy_output{sizeof(granit_render_pipeline_output),
                                                    0,
                                                    desc->output,
                                                    desc->output_format,
                                                    desc->width,
                                                    desc->height,
                                                    0,
                                                    desc->canvas,
                                                    desc->debug_draw};
  if (desc->output_count == 0 && !valid_output(legacy_output))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < desc->output_count; ++index) {
    if (!valid_output(desc->outputs[index]))
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
      const auto& render_output = desc->output_count == 0 ? legacy_output : desc->outputs[offset];
      result = render_view(*state, *desc, snapshot, bindings, desc->first_view + offset,
                           render_output, desc->frame);
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
  auto result = GRANIT_SUCCESS;
  const auto reset_draw_bindings = [&](auto& entries) {
    for (auto& entry : entries) {
      const auto reset_result = entry.bindings.reset();
      if (result == GRANIT_SUCCESS)
        result = reset_result;
      const auto lighting_result = entry.lighting.reset();
      if (result == GRANIT_SUCCESS)
        result = lighting_result;
    }
    entries.clear();
  };
  reset_draw_bindings(removed->opaque_draw_bindings);
  reset_draw_bindings(removed->shadow_draw_bindings);
  for (auto& slot : removed->metrics_slots) {
    if (slot.pool == GRANIT_NULL_HANDLE)
      continue;
    const auto metrics_result = granit_timestamp_query_pool_destroy(renderer, slot.pool);
    if (result == GRANIT_SUCCESS)
      result = metrics_result;
    slot.pool = GRANIT_NULL_HANDLE;
  }
  removed->metrics_slots.clear();
  const auto shadow_view_result = removed->shadow_view.reset();
  if (result == GRANIT_SUCCESS)
    result = static_cast<granit_result>(shadow_view_result);
  const auto shadow_texture_result = removed->shadow_texture.reset();
  if (result == GRANIT_SUCCESS)
    result = static_cast<granit_result>(shadow_texture_result);
  for (auto& pipeline_resource : removed->tone_mapping_pipelines) {
    const auto pipeline_result = pipeline_resource.reset();
    if (result == GRANIT_SUCCESS)
      result = pipeline_result;
  }
  const auto ibl_result = removed->default_ibl.reset();
  if (result == GRANIT_SUCCESS)
    result = ibl_result;
  for (const auto& entry : removed->shadow_pipelines) {
    const auto pipeline_result = granit_graphics_pipeline_destroy(renderer, entry.pipeline);
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

extern "C" granit_result granit_render_pipeline_metrics_enable(granit_renderer renderer,
                                                               granit_render_pipeline pipeline) {
  const auto state = find_pipeline(renderer, pipeline);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  if (state->metrics_enabled)
    return GRANIT_SUCCESS;
  const granit_timestamp_query_pool_desc desc{sizeof(desc), 8, 0};
  pipeline_state::metrics_slot slot;
  const auto result = granit_timestamp_query_pool_create(renderer, &desc, &slot.pool);
  if (result != GRANIT_SUCCESS)
    return result;
  try {
    state->metrics_slots.push_back(slot);
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_timestamp_query_pool_destroy(renderer, slot.pool));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    static_cast<void>(granit_timestamp_query_pool_destroy(renderer, slot.pool));
    return GRANIT_ERROR_INTERNAL;
  }
  state->metrics_enabled = true;
  return GRANIT_SUCCESS;
}

extern "C" granit_result
granit_render_pipeline_get_metrics(granit_renderer renderer, granit_render_pipeline pipeline,
                                   granit_render_pipeline_metrics* metrics) {
  if (metrics == nullptr || metrics->struct_size < GRANIT_RENDER_PIPELINE_METRICS_VERSION_1_SIZE ||
      metrics->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto state = find_pipeline(renderer, pipeline);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  if (!state->metrics_enabled || !state->metrics_available)
    return GRANIT_ERROR_NOT_READY;
  *metrics = state->metrics;
  return GRANIT_SUCCESS;
}

extern "C" granit_result
granit_render_pipeline_shadow_half_extent_set(granit_renderer renderer,
                                              granit_render_pipeline pipeline, float half_extent) {
  if (!std::isfinite(half_extent) || half_extent <= 0.0F)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto state = find_pipeline(renderer, pipeline);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  if (!state->alive)
    return GRANIT_ERROR_INVALID_HANDLE;
  state->shadow_half_extent = half_extent;
  return GRANIT_SUCCESS;
}
