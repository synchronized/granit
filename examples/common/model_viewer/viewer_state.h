// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_VIEWER_STATE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_VIEWER_STATE_H_

#include "gltf/scene.h"
#include "model_viewer/orbit_camera.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace granit::example::model_viewer {

enum class debug_display_mode : std::uint32_t {
  shaded,
  base_color,
  normals,
  metallic,
  roughness,
};

struct viewer_panels {
  bool scene{true};
  bool inspector{true};
  bool lighting{true};
  bool renderer{true};
  bool performance{true};
};

struct directional_light_state {
  math::float3 direction{0.0F, -1.0F, 1.0F};
  math::float3 radiance{3.0F, 3.0F, 3.0F};
};

struct viewer_change {
  std::optional<std::uint32_t> selected_node;
  std::optional<std::uint32_t> selected_material;
  std::optional<std::uint32_t> visibility_node;
  std::optional<bool> visible;
  std::optional<directional_light_state> directional_light;
  std::optional<float> exposure_ev;
  std::optional<float> environment_intensity;
  std::optional<float> environment_rotation_radians;
  std::optional<math::float3> background_color;
  std::optional<debug_display_mode> debug_display;
  std::optional<viewer_panels> panels;
};

enum class viewer_state_error {
  none,
  invalid_selection,
  invalid_visibility_change,
  invalid_light,
  invalid_exposure,
  invalid_environment,
  invalid_background,
  invalid_debug_display,
};

class viewer_state {
public:
  void reset(const gltf::scene& scene);
  [[nodiscard]] viewer_state_error apply(const gltf::scene& scene, const viewer_change& change);

  [[nodiscard]] std::uint32_t selected_node() const noexcept { return selected_node_; }
  [[nodiscard]] std::uint32_t selected_material() const noexcept { return selected_material_; }
  [[nodiscard]] bool node_visible(std::uint32_t node) const noexcept;
  [[nodiscard]] const std::vector<bool>& node_visibility() const noexcept {
    return node_visibility_;
  }
  [[nodiscard]] orbit_camera& camera() noexcept { return camera_; }
  [[nodiscard]] const orbit_camera& camera() const noexcept { return camera_; }
  [[nodiscard]] const directional_light_state& directional_light() const noexcept {
    return directional_light_;
  }
  [[nodiscard]] float exposure_ev() const noexcept { return exposure_ev_; }
  [[nodiscard]] float environment_intensity() const noexcept { return environment_intensity_; }
  [[nodiscard]] float environment_rotation_radians() const noexcept {
    return environment_rotation_radians_;
  }
  [[nodiscard]] math::float3 background_color() const noexcept { return background_color_; }
  [[nodiscard]] debug_display_mode debug_display() const noexcept { return debug_display_; }
  [[nodiscard]] const viewer_panels& panels() const noexcept { return panels_; }

private:
  std::uint32_t selected_node_{gltf::invalid_index};
  std::uint32_t selected_material_{gltf::invalid_index};
  std::vector<bool> node_visibility_;
  orbit_camera camera_;
  directional_light_state directional_light_;
  float exposure_ev_{};
  float environment_intensity_{0.15F};
  float environment_rotation_radians_{};
  math::float3 background_color_{0.025F, 0.04F, 0.065F};
  debug_display_mode debug_display_{debug_display_mode::shaded};
  viewer_panels panels_;
};

} // namespace granit::example::model_viewer

#endif
