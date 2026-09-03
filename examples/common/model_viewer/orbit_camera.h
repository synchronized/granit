// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ORBIT_CAMERA_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ORBIT_CAMERA_H_

#include "viewer_input.h"

#include "math/math.h"

#include <cstdint>

namespace granit::example::model_viewer {

struct camera_bounds {
  math::float3 center{};
  float radius{};
};

struct camera_matrices {
  math::matrix4 view{math::identity_matrix4};
  math::matrix4 projection{math::identity_matrix4};
  math::matrix4 view_projection{math::identity_matrix4};
  math::float3 position{};
};

class orbit_camera {
public:
  orbit_camera() noexcept;

  [[nodiscard]] bool focus(camera_bounds bounds, std::uint32_t width,
                           std::uint32_t height) noexcept;
  [[nodiscard]] bool update(const viewer_input& input, std::uint32_t width, std::uint32_t height,
                            const camera_bounds* focus_bounds = nullptr) noexcept;
  [[nodiscard]] bool matrices(std::uint32_t width, std::uint32_t height,
                              camera_matrices& output) const noexcept;

  [[nodiscard]] math::float3 target() const noexcept { return target_; }
  [[nodiscard]] float distance() const noexcept { return distance_; }
  [[nodiscard]] float yaw() const noexcept { return yaw_; }
  [[nodiscard]] float pitch() const noexcept { return pitch_; }
  [[nodiscard]] float near_plane() const noexcept { return near_plane_; }
  [[nodiscard]] float far_plane() const noexcept { return far_plane_; }

private:
  void save_home() noexcept;
  void restore_home() noexcept;

  math::float3 target_{};
  float distance_{3.0F};
  float yaw_{};
  float pitch_{};
  float vertical_fov_{0.785398163F};
  float near_plane_{0.01F};
  float far_plane_{100.0F};
  math::float3 home_target_{};
  float home_distance_{3.0F};
  float home_yaw_{};
  float home_pitch_{};
  float home_near_{0.01F};
  float home_far_{100.0F};
};

} // namespace granit::example::model_viewer

#endif
