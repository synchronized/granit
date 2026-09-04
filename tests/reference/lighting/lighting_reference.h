// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_LIGHTING_REFERENCE_H
#define GRANIT_LIGHTING_LIGHTING_REFERENCE_H

#include "reference/material/pbr_reference.h"
#include "scene/scene_submission.h"

namespace granit::lighting {

struct lighting_surface {
  material::pbr_material_parameters material;
  material::pbr_float3 position{};
  material::pbr_float3 normal{0.0F, 0.0F, 1.0F};
  material::pbr_float3 view_direction{0.0F, 0.0F, 1.0F};
};

/** 平滑半径截止的平方反比衰减；距离不小于半径时返回零。 */
[[nodiscard]] float distance_attenuation(float distance_squared, float radius) noexcept;

/** 根据光线方向与内外锥角计算聚光响应。 */
[[nodiscard]] float spot_angle_attenuation(material::pbr_float3 light_to_surface_direction,
                                           material::pbr_float3 spot_direction, float inner_angle,
                                           float outer_angle) noexcept;

[[nodiscard]] material::pbr_float3
evaluate_directional_light(const lighting_surface& surface,
                           const scene::directional_light_input& light) noexcept;

[[nodiscard]] material::pbr_float3
evaluate_point_light(const lighting_surface& surface,
                     const scene::point_light_input& light) noexcept;

[[nodiscard]] material::pbr_float3
evaluate_spot_light(const lighting_surface& surface, const scene::spot_light_input& light) noexcept;

} // namespace granit::lighting

#endif
