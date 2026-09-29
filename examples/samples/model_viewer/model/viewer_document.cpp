// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model/viewer_document.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace granit::example::model_viewer {
namespace {

camera::camera_bounds scene_bounds(const gltf_rendering::scene_plan& plan,
                                   std::uint32_t selected_node) noexcept {
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

void viewer_document::assign(gltf::scene scene) {
  state_.reset(scene);
  scene_ = std::move(scene);
  performance_.clear();
  camera_initialized_ = false;
}

void viewer_document::clear() noexcept {
  scene_ = {};
  state_ = {};
  performance_.clear();
  camera_initialized_ = false;
}

granit::result viewer_document::update(const viewer_document_update& input,
                                       const gltf_rendering::scene_plan& plan,
                                       viewer_document_frame& output) {
  if (input.width == 0 || input.height == 0)
    return granit::result::not_ready;
  if (state_.apply(scene_, input.change) != viewer_state_error::none)
    return granit::result::invalid_argument;

  const auto whole_scene_bounds = scene_bounds(plan, gltf::invalid_index);
  if (!camera_initialized_) {
    if (!state_.camera().focus(whole_scene_bounds, input.width, input.height))
      return granit::result::invalid_argument;
    camera_initialized_ = true;
  }
  const auto selected_bounds = scene_bounds(plan, state_.selected_node());
  if (!state_.camera().update(input.input, input.width, input.height, &selected_bounds))
    return granit::result::invalid_argument;

  camera::camera_matrices matrices;
  if (!state_.camera().matrices(input.width, input.height, matrices))
    return granit::result::invalid_argument;
  viewer_document_frame candidate;
  candidate.view = {.view = matrices.view,
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
  candidate.directional_light = {
      .direction_to_light = {-light_direction.x, -light_direction.y, -light_direction.z},
      .radiance = light_state.radiance,
      .layer_mask = std::numeric_limits<std::uint64_t>::max()};
  candidate.exposure_ev = state_.exposure_ev();
  candidate.environment_intensity = state_.environment_intensity();
  candidate.environment_rotation_radians = state_.environment_rotation_radians();
  const auto background = state_.background_color();
  candidate.clear_color = {background.x, background.y, background.z, 1.0F};
  if (input.performance)
    performance_.push(*input.performance);
  output = candidate;
  return granit::result::success;
}

granit::result
viewer_document::update_material(std::uint32_t material_index,
                                 const gltf_rendering::material_factor_update& edit) noexcept {
  if (material_index >= scene_.materials.size())
    return granit::result::invalid_argument;
  auto& material = scene_.materials[material_index];
  material.base_color = edit.base_color;
  material.metallic = edit.metallic;
  material.roughness = edit.roughness;
  material.normal_scale = edit.normal_scale;
  material.occlusion_strength = edit.occlusion_strength;
  material.emissive = edit.emissive;
  return granit::result::success;
}

} // namespace granit::example::model_viewer
