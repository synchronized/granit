// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "reference/lighting/lighting_reference.h"

#include <algorithm>
#include <cmath>

namespace granit::lighting {
namespace {

using material::pbr_float3;

pbr_float3 to_pbr(scene::float3 value) noexcept { return {value.x, value.y, value.z}; }

pbr_float3 evaluate(const lighting_surface& surface, pbr_float3 direction,
                    pbr_float3 radiance) noexcept {
  return material::evaluate_pbr_direct_light(surface.material,
                                             {.normal = surface.normal,
                                              .view_direction = surface.view_direction,
                                              .light_direction = direction,
                                              .radiance = radiance});
}

} // namespace

float distance_attenuation(float distance_squared, float radius) noexcept {
  if (!(radius > 0.0F) || !(distance_squared >= 0.0F) || distance_squared >= radius * radius)
    return 0.0F;
  const auto normalized_squared = distance_squared / (radius * radius);
  const auto smooth = std::max(1.0F - normalized_squared * normalized_squared, 0.0F);
  return smooth * smooth / std::max(distance_squared, 0.0001F);
}

float spot_angle_attenuation(pbr_float3 light_to_surface_direction, pbr_float3 spot_direction,
                             float inner_angle, float outer_angle) noexcept {
  const auto cosine =
      math::dot(math::normalize(light_to_surface_direction), math::normalize(spot_direction));
  const auto inner_cosine = std::cos(inner_angle);
  const auto outer_cosine = std::cos(outer_angle);
  const auto width = inner_cosine - outer_cosine;
  if (!(width > 0.0F))
    return cosine >= outer_cosine ? 1.0F : 0.0F;
  const auto value = std::clamp((cosine - outer_cosine) / width, 0.0F, 1.0F);
  return value * value * (3.0F - 2.0F * value);
}

pbr_float3 evaluate_directional_light(const lighting_surface& surface,
                                      const scene::directional_light_input& light) noexcept {
  return evaluate(surface, to_pbr(light.direction_to_light), to_pbr(light.radiance));
}

pbr_float3 evaluate_point_light(const lighting_surface& surface,
                                const scene::point_light_input& light) noexcept {
  const auto to_light = math::subtract(to_pbr(light.position), surface.position);
  const auto distance_squared = math::dot(to_light, to_light);
  const auto attenuation = distance_attenuation(distance_squared, light.radius);
  if (attenuation <= 0.0F)
    return {};
  return evaluate(surface, math::normalize(to_light),
                  math::multiply(to_pbr(light.intensity), attenuation));
}

pbr_float3 evaluate_spot_light(const lighting_surface& surface,
                               const scene::spot_light_input& light) noexcept {
  const auto to_light = math::subtract(to_pbr(light.position), surface.position);
  const auto distance_squared = math::dot(to_light, to_light);
  const auto distance_factor = distance_attenuation(distance_squared, light.radius);
  if (distance_factor <= 0.0F)
    return {};
  const auto light_to_surface = math::normalize(math::multiply(to_light, -1.0F));
  const auto angle_factor = spot_angle_attenuation(light_to_surface, to_pbr(light.direction),
                                                   light.inner_angle, light.outer_angle);
  if (angle_factor <= 0.0F)
    return {};
  return evaluate(surface, math::normalize(to_light),
                  math::multiply(to_pbr(light.intensity), distance_factor * angle_factor));
}

} // namespace granit::lighting
