// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_REFERENCE_H
#define GRANIT_MATERIAL_PBR_REFERENCE_H

#include "material/pbr_types.h"

namespace granit::material {

[[nodiscard]] pbr_float3 pbr_fresnel_schlick(float view_dot_half,
                                             pbr_float3 reflectance_at_normal) noexcept;

[[nodiscard]] float pbr_distribution_ggx(float normal_dot_half,
                                         float perceptual_roughness) noexcept;

[[nodiscard]] float pbr_visibility_smith_correlated(float normal_dot_view, float normal_dot_light,
                                                    float perceptual_roughness) noexcept;

/** 计算单个方向光的线性 RGB 直接光照，不包含环境光、遮蔽和自发光。 */
[[nodiscard]] pbr_float3 evaluate_pbr_direct_light(const pbr_material_parameters& material,
                                                   const pbr_direct_light_input& input) noexcept;

} // namespace granit::material

#endif
