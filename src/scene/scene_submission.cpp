// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/scene_submission.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <numbers>
#include <utility>

namespace granit::scene {
namespace {

bool finite(float3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite(const matrix4& value) noexcept {
  return std::ranges::all_of(value, [](float component) { return std::isfinite(component); });
}

bool nonnegative(float3 value) noexcept {
  return value.x >= 0.0F && value.y >= 0.0F && value.z >= 0.0F;
}

bool normalize_in_place(float3& value) noexcept {
  const auto normalized = math::normalize(value);
  if (normalized == float3{})
    return false;
  value = normalized;
  return true;
}

submission_error validate_view(const view_input& view) noexcept {
  if (!finite(view.view) || !finite(view.projection) || !finite(view.view_projection) ||
      !finite(view.camera_position) || !std::isfinite(view.area.x) || !std::isfinite(view.area.y) ||
      !std::isfinite(view.area.width) || !std::isfinite(view.area.height)) {
    return submission_error::non_finite_value;
  }
  if (view.area.width <= 0.0F || view.area.height <= 0.0F)
    return submission_error::invalid_viewport;
  return view.layer_mask == 0 ? submission_error::invalid_layer_mask : submission_error::none;
}

submission_error validate_renderable(const renderable_input& value) noexcept {
  if (!finite(value.model) || !finite(value.normal_matrix) || !finite(value.bounds.center) ||
      !std::isfinite(value.bounds.radius)) {
    return submission_error::non_finite_value;
  }
  if (value.bounds.radius < 0.0F)
    return submission_error::invalid_bounds;
  return value.layer_mask == 0 ? submission_error::invalid_layer_mask : submission_error::none;
}

submission_error validate_directional(directional_light_input& value) noexcept {
  if (!finite(value.direction_to_light) || !finite(value.radiance))
    return submission_error::non_finite_value;
  if (!normalize_in_place(value.direction_to_light))
    return submission_error::invalid_direction;
  if (!nonnegative(value.radiance))
    return submission_error::negative_light_value;
  return value.layer_mask == 0 ? submission_error::invalid_layer_mask : submission_error::none;
}

submission_error validate_point(const point_light_input& value) noexcept {
  if (!finite(value.position) || !finite(value.intensity) || !std::isfinite(value.radius))
    return submission_error::non_finite_value;
  if (!nonnegative(value.intensity))
    return submission_error::negative_light_value;
  if (value.radius <= 0.0F)
    return submission_error::invalid_light_radius;
  return value.layer_mask == 0 ? submission_error::invalid_layer_mask : submission_error::none;
}

submission_error validate_spot(spot_light_input& value) noexcept {
  if (!finite(value.position) || !finite(value.direction) || !finite(value.intensity) ||
      !std::isfinite(value.radius) || !std::isfinite(value.inner_angle) ||
      !std::isfinite(value.outer_angle)) {
    return submission_error::non_finite_value;
  }
  if (!normalize_in_place(value.direction))
    return submission_error::invalid_direction;
  if (!nonnegative(value.intensity))
    return submission_error::negative_light_value;
  if (value.radius <= 0.0F)
    return submission_error::invalid_light_radius;
  if (value.inner_angle < 0.0F || value.outer_angle < value.inner_angle ||
      value.outer_angle >= std::numbers::pi_v<float> * 0.5F) {
    return submission_error::invalid_spot_cone;
  }
  return value.layer_mask == 0 ? submission_error::invalid_layer_mask : submission_error::none;
}

} // namespace

submission_error build_frame_snapshot(const frame_submission& submission,
                                      frame_snapshot& output) noexcept {
  const auto view_error = validate_view(submission.view);
  if (view_error != submission_error::none)
    return view_error;
  try {
    frame_snapshot candidate;
    candidate.view_ = submission.view;
    candidate.renderables_.assign(submission.renderables.begin(), submission.renderables.end());
    candidate.directional_lights_.assign(submission.directional_lights.begin(),
                                         submission.directional_lights.end());
    candidate.point_lights_.assign(submission.point_lights.begin(), submission.point_lights.end());
    candidate.spot_lights_.assign(submission.spot_lights.begin(), submission.spot_lights.end());
    for (const auto& value : candidate.renderables_) {
      const auto error = validate_renderable(value);
      if (error != submission_error::none)
        return error;
    }
    for (auto& value : candidate.directional_lights_) {
      const auto error = validate_directional(value);
      if (error != submission_error::none)
        return error;
    }
    for (const auto& value : candidate.point_lights_) {
      const auto error = validate_point(value);
      if (error != submission_error::none)
        return error;
    }
    for (auto& value : candidate.spot_lights_) {
      const auto error = validate_spot(value);
      if (error != submission_error::none)
        return error;
    }
    output = std::move(candidate);
    return submission_error::none;
  } catch (const std::bad_alloc&) {
    return submission_error::out_of_memory;
  } catch (...) {
    return submission_error::out_of_memory;
  }
}

} // namespace granit::scene
