// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_state.h"

#include <cmath>

namespace granit::example::model_viewer {
namespace {

bool valid_index(std::uint32_t value, std::size_t size) noexcept {
  return value == gltf::invalid_index || value < size;
}

bool valid_vector(const math::float3& value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool valid_light(const directional_light_state& light) noexcept {
  const auto length_squared = light.direction.x * light.direction.x +
                              light.direction.y * light.direction.y +
                              light.direction.z * light.direction.z;
  return valid_vector(light.direction) && length_squared > 0.000001F &&
         valid_vector(light.radiance) && light.radiance.x >= 0.0F && light.radiance.y >= 0.0F &&
         light.radiance.z >= 0.0F;
}

bool valid_debug_display(debug_display_mode mode) noexcept {
  return mode >= debug_display_mode::shaded && mode <= debug_display_mode::roughness;
}

} // namespace

void viewer_state::reset(const gltf::scene& scene) {
  selected_node_ = gltf::invalid_index;
  selected_material_ = gltf::invalid_index;
  node_visibility_.assign(scene.nodes.size(), true);
  camera_ = {};
  directional_light_ = {};
  exposure_ev_ = 0.0F;
  environment_intensity_ = 1.0F;
  environment_rotation_radians_ = 0.0F;
  background_color_ = {0.025F, 0.04F, 0.065F};
  debug_display_ = debug_display_mode::shaded;
  panels_ = {};
}

viewer_state_error viewer_state::apply(const gltf::scene& scene, const viewer_change& change) {
  if ((change.selected_node && !valid_index(*change.selected_node, scene.nodes.size())) ||
      (change.selected_material && !valid_index(*change.selected_material, scene.materials.size())))
    return viewer_state_error::invalid_selection;
  if (change.visibility_node.has_value() != change.visible.has_value() ||
      (change.visibility_node && *change.visibility_node >= scene.nodes.size()))
    return viewer_state_error::invalid_visibility_change;
  if (change.directional_light && !valid_light(*change.directional_light))
    return viewer_state_error::invalid_light;
  if (change.exposure_ev && (!std::isfinite(*change.exposure_ev) || *change.exposure_ev < -24.0F ||
                             *change.exposure_ev > 24.0F))
    return viewer_state_error::invalid_exposure;
  if ((change.environment_intensity &&
       (!std::isfinite(*change.environment_intensity) || *change.environment_intensity < 0.0F ||
        *change.environment_intensity > 100.0F)) ||
      (change.environment_rotation_radians && !std::isfinite(*change.environment_rotation_radians)))
    return viewer_state_error::invalid_environment;
  if (change.background_color &&
      (!valid_vector(*change.background_color) || change.background_color->x < 0.0F ||
       change.background_color->x > 1.0F || change.background_color->y < 0.0F ||
       change.background_color->y > 1.0F || change.background_color->z < 0.0F ||
       change.background_color->z > 1.0F))
    return viewer_state_error::invalid_background;
  if (change.debug_display && !valid_debug_display(*change.debug_display))
    return viewer_state_error::invalid_debug_display;

  // Scene 替换后先收敛旧选择与可见性，再一次性提交本次有效变更。
  if (node_visibility_.size() != scene.nodes.size())
    node_visibility_.assign(scene.nodes.size(), true);
  if (!valid_index(selected_node_, scene.nodes.size()))
    selected_node_ = gltf::invalid_index;
  if (!valid_index(selected_material_, scene.materials.size()))
    selected_material_ = gltf::invalid_index;
  if (change.selected_node)
    selected_node_ = *change.selected_node;
  if (change.selected_material)
    selected_material_ = *change.selected_material;
  if (change.visibility_node)
    node_visibility_[*change.visibility_node] = *change.visible;
  if (change.directional_light)
    directional_light_ = *change.directional_light;
  if (change.exposure_ev)
    exposure_ev_ = *change.exposure_ev;
  if (change.environment_intensity)
    environment_intensity_ = *change.environment_intensity;
  if (change.environment_rotation_radians)
    environment_rotation_radians_ = *change.environment_rotation_radians;
  if (change.background_color)
    background_color_ = *change.background_color;
  if (change.debug_display)
    debug_display_ = *change.debug_display;
  if (change.panels)
    panels_ = *change.panels;
  return viewer_state_error::none;
}

bool viewer_state::node_visible(std::uint32_t node) const noexcept {
  return node < node_visibility_.size() && node_visibility_[node];
}

} // namespace granit::example::model_viewer
