// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SCENE_SCENE_SUBMISSION_H
#define GRANIT_SCENE_SCENE_SUBMISSION_H

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace granit::scene {

struct float3 {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;

  friend bool operator==(const float3&, const float3&) = default;
};

using matrix4 = std::array<float, 16>;

struct viewport {
  float x = 0.0F;
  float y = 0.0F;
  float width = 1.0F;
  float height = 1.0F;
};

struct view_input {
  matrix4 view{};
  matrix4 projection{};
  matrix4 view_projection{};
  float3 camera_position{};
  viewport area{};
  std::uint64_t layer_mask = UINT64_MAX;
};

struct bounding_sphere {
  float3 center{};
  float radius = 0.0F;
};

struct renderable_input {
  matrix4 model{};
  matrix4 normal_matrix{};
  bounding_sphere bounds{};
  std::uint64_t layer_mask = UINT64_MAX;
  std::uint64_t sort_key = 0;
  std::uint64_t payload = 0;
  std::uint32_t object_id = 0;
};

struct directional_light_input {
  float3 direction_to_light{0.0F, 0.0F, 1.0F};
  float3 radiance{1.0F, 1.0F, 1.0F};
  std::uint64_t layer_mask = UINT64_MAX;
};

struct point_light_input {
  float3 position{};
  float3 intensity{1.0F, 1.0F, 1.0F};
  float radius = 1.0F;
  std::uint64_t layer_mask = UINT64_MAX;
};

struct spot_light_input {
  float3 position{};
  float3 direction{0.0F, 0.0F, -1.0F};
  float3 intensity{1.0F, 1.0F, 1.0F};
  float radius = 1.0F;
  float inner_angle = 0.0F;
  float outer_angle = 0.785398163F;
  std::uint64_t layer_mask = UINT64_MAX;
};

struct frame_submission {
  view_input view;
  std::span<const renderable_input> renderables;
  std::span<const directional_light_input> directional_lights;
  std::span<const point_light_input> point_lights;
  std::span<const spot_light_input> spot_lights;
};

enum class submission_error : std::uint8_t {
  none,
  non_finite_value,
  invalid_viewport,
  invalid_layer_mask,
  invalid_bounds,
  invalid_direction,
  negative_light_value,
  invalid_light_radius,
  invalid_spot_cone,
  out_of_memory,
};

class frame_snapshot {
public:
  [[nodiscard]] const view_input& view() const noexcept { return view_; }
  [[nodiscard]] std::span<const renderable_input> renderables() const noexcept {
    return renderables_;
  }
  [[nodiscard]] std::span<const directional_light_input> directional_lights() const noexcept {
    return directional_lights_;
  }
  [[nodiscard]] std::span<const point_light_input> point_lights() const noexcept {
    return point_lights_;
  }
  [[nodiscard]] std::span<const spot_light_input> spot_lights() const noexcept {
    return spot_lights_;
  }

private:
  friend submission_error build_frame_snapshot(const frame_submission&, frame_snapshot&) noexcept;

  view_input view_{};
  std::vector<renderable_input> renderables_;
  std::vector<directional_light_input> directional_lights_;
  std::vector<point_light_input> point_lights_;
  std::vector<spot_light_input> spot_lights_;
};

/** 校验并复制一次单 View 场景提交；失败时不修改 output。 */
[[nodiscard]] submission_error build_frame_snapshot(const frame_submission& submission,
                                                    frame_snapshot& output) noexcept;

} // namespace granit::scene

#endif
