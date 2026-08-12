// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_reference.h"

#include <cmath>

namespace granit::lighting {

shadow_projection project_directional_shadow(const math::matrix4& light_view_projection,
                                             math::float3 world_position, math::float3 normal,
                                             float normal_bias, float depth_bias) noexcept {
  if (!math::is_finite(light_view_projection) || !math::is_finite(world_position) ||
      !math::is_finite(normal) || !std::isfinite(normal_bias) || !std::isfinite(depth_bias) ||
      normal_bias < 0.0F || depth_bias < 0.0F)
    return {};
  const auto biased =
      math::add(world_position, math::multiply(math::normalize(normal), normal_bias));
  const auto clip = math::transform(light_view_projection, {biased.x, biased.y, biased.z, 1.0F});
  if (!std::isfinite(clip.w) || clip.w <= 0.0F)
    return {};
  const auto projected = math::float3{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
  if (!math::is_finite(projected))
    return {};
  const auto uv = math::float2{projected.x * 0.5F + 0.5F, projected.y * -0.5F + 0.5F};
  const auto inside = uv.x >= 0.0F && uv.x <= 1.0F && uv.y >= 0.0F && uv.y <= 1.0F &&
                      projected.z >= 0.0F && projected.z <= 1.0F;
  return {.uv = uv, .comparison_depth = projected.z - depth_bias, .inside = inside};
}

float evaluate_shadow_compare(const shadow_projection& projection, float stored_depth) noexcept {
  if (!projection.inside)
    return 1.0F;
  if (!std::isfinite(projection.comparison_depth) || !std::isfinite(stored_depth))
    return 0.0F;
  return projection.comparison_depth <= stored_depth ? 1.0F : 0.0F;
}

} // namespace granit::lighting
