// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/application_core.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace granit::example::model_viewer {
namespace {

camera_bounds scene_bounds(const gpu_scene_plan& plan, std::uint32_t selected_node) noexcept {
  math::float3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max()};
  math::float3 maximum{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                       -std::numeric_limits<float>::max()};
  bool found = false;
  for (const auto& draw : plan.draws) {
    if (selected_node != gltf::invalid_index && draw.node != selected_node)
      continue;
    minimum.x = std::min(minimum.x, draw.bounds_center.x - draw.bounds_radius);
    minimum.y = std::min(minimum.y, draw.bounds_center.y - draw.bounds_radius);
    minimum.z = std::min(minimum.z, draw.bounds_center.z - draw.bounds_radius);
    maximum.x = std::max(maximum.x, draw.bounds_center.x + draw.bounds_radius);
    maximum.y = std::max(maximum.y, draw.bounds_center.y + draw.bounds_radius);
    maximum.z = std::max(maximum.z, draw.bounds_center.z + draw.bounds_radius);
    found = true;
  }
  if (!found)
    return {{}, 1.0F};
  const math::float3 center{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F,
                            (minimum.z + maximum.z) * 0.5F};
  const auto extent = math::subtract(maximum, center);
  return {center, std::max(math::length(extent), 0.001F)};
}

} // namespace

granit::result application_core::begin_renderer() noexcept {
  if (phase_ != application_phase::platform_ready)
    return granit::result::invalid_argument;
  phase_ = application_phase::renderer_pending;
  return granit::result::success;
}

granit::result application_core::renderer_ready() noexcept {
  if (phase_ != application_phase::renderer_pending)
    return granit::result::invalid_argument;
  phase_ = application_phase::asset_loading;
  return granit::result::success;
}

granit::result application_core::load_asset(std::span<const std::byte> bytes,
                                            const gltf::resource_resolver* resolver) {
  if (phase_ != application_phase::asset_loading)
    return granit::result::invalid_argument;
  gltf::scene candidate;
  const auto loaded = gltf::load(bytes, resolver, candidate);
  if (!loaded) {
    fail(granit::result::invalid_argument, loaded.diagnostic);
    return failure_result_;
  }
  return accept_scene(std::move(candidate));
}

granit::result application_core::accept_scene(gltf::scene scene) {
  if (phase_ != application_phase::asset_loading)
    return granit::result::invalid_argument;
  try {
    state_.reset(scene);
    cpu_scene_ = std::move(scene);
    phase_ = application_phase::gpu_upload;
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    fail(granit::result::out_of_memory, "模型查看器 CPU Scene 分配失败");
    return failure_result_;
  }
}

granit::result application_core::upload(granit_renderer renderer,
                                        std::span<const std::byte> environment_bytes,
                                        float sampler_anisotropy) {
  if (phase_ != application_phase::gpu_upload)
    return granit::result::invalid_argument;
  const auto result = gpu_scene_.initialize(renderer, cpu_scene_, sampler_anisotropy);
  if (granit::failed(result)) {
    fail(result, "模型查看器 GPU Scene 上传失败");
    return result;
  }
  granit::result environment_result;
  if (environment_bytes.empty()) {
    environment_result = environment_.initialize_builtin_studio(renderer);
  } else {
    environment_package package;
    if (parse_environment_package(environment_bytes, package) != environment_package_error::none) {
      gpu_scene_.reset();
      fail(granit::result::invalid_argument, "模型查看器环境包解析失败");
      return granit::result::invalid_argument;
    }
    environment_result = environment_.initialize(renderer, package);
  }
  if (granit::failed(environment_result)) {
    gpu_scene_.reset();
    fail(environment_result, "模型查看器内建环境上传失败");
    return environment_result;
  }
  phase_ = application_phase::ready;
  return granit::result::success;
}

granit::result application_core::reupload_scene(granit_renderer renderer,
                                                float sampler_anisotropy) {
  if (phase_ != application_phase::ready)
    return granit::result::invalid_argument;
  return gpu_scene_.initialize(renderer, cpu_scene_, sampler_anisotropy);
}

granit::result application_core::tick(const application_tick_input& input,
                                      application_tick_output& output) {
  if (phase_ != application_phase::ready)
    return granit::result::invalid_argument;
  if (input.width == 0 || input.height == 0)
    return granit::result::not_ready;
  if (state_.apply(cpu_scene_, input.change) != viewer_state_error::none)
    return granit::result::invalid_argument;
  if (input.change.debug_display) {
    const auto debug_result =
        gpu_scene_.update_debug_display(static_cast<std::uint32_t>(*input.change.debug_display));
    if (granit::failed(debug_result))
      return debug_result;
  }

  const auto whole_scene_bounds = scene_bounds(gpu_scene_.plan(), gltf::invalid_index);
  if (!camera_initialized_) {
    if (!state_.camera().focus(whole_scene_bounds, input.width, input.height))
      return granit::result::invalid_argument;
    camera_initialized_ = true;
  }
  const auto selected_bounds = scene_bounds(gpu_scene_.plan(), state_.selected_node());
  if (!state_.camera().update(input.input, input.width, input.height, &selected_bounds))
    return granit::result::invalid_argument;

  camera_matrices matrices;
  if (!state_.camera().matrices(input.width, input.height, matrices))
    return granit::result::invalid_argument;
  const granit_scene_view view{.view = matrices.view,
                               .projection = matrices.projection,
                               .view_projection = matrices.view_projection,
                               .camera_position = matrices.position,
                               .viewport_x = 0.0F,
                               .viewport_y = 0.0F,
                               .viewport_width = static_cast<float>(input.width),
                               .viewport_height = static_cast<float>(input.height),
                               .layer_mask = std::numeric_limits<std::uint64_t>::max()};
  const auto& light_state = state_.directional_light();
  const auto camera_forward =
      math::normalize(math::subtract(state_.camera().target(), matrices.position));
  const auto camera_right = math::normalize(math::cross(camera_forward, {0.0F, 1.0F, 0.0F}));
  const auto camera_up = math::cross(camera_right, camera_forward);
  const auto light_direction =
      math::normalize(math::add(math::add(math::multiply(camera_right, light_state.direction.x),
                                          math::multiply(camera_up, light_state.direction.y)),
                                math::multiply(camera_forward, light_state.direction.z)));
  const granit_scene_directional_light light{
      .direction_to_light = {-light_direction.x, -light_direction.y, -light_direction.z},
      .radiance = light_state.radiance,
      .layer_mask = std::numeric_limits<std::uint64_t>::max()};

  application_tick_output candidate;
  const auto snapshot_result = gpu_scene_.create_snapshot(std::span{&view, 1}, std::span{&light, 1},
                                                          {}, {}, candidate.snapshot);
  if (granit::failed(snapshot_result))
    return snapshot_result;
  candidate.render.scene = candidate.snapshot.native_handle();
  candidate.render.width = input.width;
  candidate.render.height = input.height;
  candidate.render.exposure_ev = state_.exposure_ev();
  const auto background = state_.background_color();
  candidate.render.clear_color = {background.x, background.y, background.z, 1.0F};
  environment_.environment().intensity = state_.environment_intensity();
  environment_.environment().rotation_radians = state_.environment_rotation_radians();
  candidate.render.environment = &environment_.environment();
  candidate.render.draw_binding_count =
      static_cast<std::uint32_t>(gpu_scene_.draw_bindings().size());
  candidate.render.draw_bindings = gpu_scene_.draw_bindings().data();
  if (input.performance)
    performance_.push(*input.performance);
  output = std::move(candidate);
  return granit::result::success;
}

void application_core::fail(granit::result result, std::string diagnostic) {
  gpu_scene_.reset();
  failure_result_ = result;
  diagnostic_ = std::move(diagnostic);
  phase_ = application_phase::failed;
}

void application_core::reset() noexcept {
  gpu_scene_.reset();
  environment_.reset();
  cpu_scene_ = {};
  state_ = {};
  performance_.clear();
  camera_initialized_ = false;
  diagnostic_.clear();
  failure_result_ = granit::result::success;
  phase_ = application_phase::platform_ready;
}

} // namespace granit::example::model_viewer
