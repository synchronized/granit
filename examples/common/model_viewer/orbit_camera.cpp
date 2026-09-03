// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/orbit_camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace granit::example::model_viewer {
namespace {

constexpr float minimum_pitch_margin = 0.01F;
constexpr float minimum_distance = 0.0001F;

bool valid_extent(std::uint32_t width, std::uint32_t height) noexcept {
  return width != 0 && height != 0;
}

math::float3 orbit_direction(float yaw, float pitch) noexcept {
  const auto horizontal = std::cos(pitch);
  return {horizontal * std::sin(yaw), std::sin(pitch), horizontal * std::cos(yaw)};
}

} // namespace

orbit_camera::orbit_camera() noexcept { save_home(); }

void orbit_camera::save_home() noexcept {
  home_target_ = target_;
  home_distance_ = distance_;
  home_yaw_ = yaw_;
  home_pitch_ = pitch_;
  home_near_ = near_plane_;
  home_far_ = far_plane_;
}

void orbit_camera::restore_home() noexcept {
  target_ = home_target_;
  distance_ = home_distance_;
  yaw_ = home_yaw_;
  pitch_ = home_pitch_;
  near_plane_ = home_near_;
  far_plane_ = home_far_;
}

bool orbit_camera::focus(camera_bounds bounds, std::uint32_t width, std::uint32_t height) noexcept {
  if (!valid_extent(width, height) || !math::is_finite(bounds.center) ||
      !std::isfinite(bounds.radius) || bounds.radius < 0.0F)
    return false;
  const auto radius = std::max(bounds.radius, minimum_distance);
  const auto aspect = static_cast<float>(width) / static_cast<float>(height);
  const auto half_vertical = vertical_fov_ * 0.5F;
  const auto half_horizontal = std::atan(std::tan(half_vertical) * aspect);
  const auto limiting_half_fov = std::min(half_vertical, half_horizontal);
  const auto candidate_distance = radius * 1.1F / std::sin(limiting_half_fov);
  if (!std::isfinite(candidate_distance) || candidate_distance <= 0.0F)
    return false;
  target_ = bounds.center;
  distance_ = candidate_distance;
  near_plane_ = std::max(radius * 0.001F, minimum_distance);
  far_plane_ = std::max(near_plane_ * 2.0F, distance_ + radius * 4.0F);
  save_home();
  return true;
}

bool orbit_camera::update(const viewer_input& input, std::uint32_t width, std::uint32_t height,
                          const camera_bounds* focus_bounds) noexcept {
  if (!valid_extent(width, height))
    return false;
  if (input.home_requested && !input.keyboard_captured)
    restore_home();
  if (input.focus_requested && !input.keyboard_captured && focus_bounds != nullptr &&
      !focus(*focus_bounds, width, height)) {
    return false;
  }
  if (!input.window_focused || !input.pointer_inside || input.mouse_captured)
    return true;
  const auto inverse_height = 1.0F / static_cast<float>(height);
  if (input.orbiting) {
    yaw_ -= input.pointer_delta_x * inverse_height * std::numbers::pi_v<float>;
    pitch_ -= input.pointer_delta_y * inverse_height * std::numbers::pi_v<float>;
    const auto limit = std::numbers::pi_v<float> * 0.5F - minimum_pitch_margin;
    pitch_ = std::clamp(pitch_, -limit, limit);
  }
  if (input.panning) {
    const auto direction = orbit_direction(yaw_, pitch_);
    const auto forward = math::multiply(direction, -1.0F);
    const auto right = math::normalize(math::cross(forward, {0, 1, 0}));
    const auto up = math::cross(right, forward);
    const auto scale = 2.0F * distance_ * std::tan(vertical_fov_ * 0.5F) * inverse_height;
    target_ = math::add(target_, math::multiply(right, -input.pointer_delta_x * scale));
    target_ = math::add(target_, math::multiply(up, input.pointer_delta_y * scale));
  }
  if (std::isfinite(input.wheel_delta)) {
    distance_ *= std::exp(-input.wheel_delta * 0.1F);
    distance_ =
        std::clamp(distance_, std::max(minimum_distance, near_plane_ * 1.01F), far_plane_ * 0.95F);
  }
  return math::is_finite(target_) && std::isfinite(distance_) && std::isfinite(yaw_) &&
         std::isfinite(pitch_);
}

bool orbit_camera::matrices(std::uint32_t width, std::uint32_t height,
                            camera_clip_space clip_space,
                            camera_matrices& output) const noexcept {
  if (!valid_extent(width, height))
    return false;
  const auto position =
      math::add(target_, math::multiply(orbit_direction(yaw_, pitch_), distance_));
  camera_matrices candidate;
  candidate.position = position;
  if (!math::look_at_rh(position, target_, {0, 1, 0}, candidate.view) ||
      !math::perspective_rh_zo(vertical_fov_,
                               static_cast<float>(width) / static_cast<float>(height), near_plane_,
                               far_plane_, candidate.projection)) {
    return false;
  }
  // 两种后端都使用左上角窗口原点，但 NDC Y 映射相反；差异在相机边界一次性收敛。
  if (clip_space == camera_clip_space::vulkan)
    candidate.projection[5] = -candidate.projection[5];
  candidate.view_projection = math::multiply(candidate.projection, candidate.view);
  output = candidate;
  return true;
}

} // namespace granit::example::model_viewer
