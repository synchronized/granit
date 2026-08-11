// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_REFERENCE_H
#define GRANIT_MATERIAL_PBR_REFERENCE_H

namespace granit::material {

struct pbr_float3 {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;

  friend bool operator==(const pbr_float3&, const pbr_float3&) = default;
};

struct pbr_material_parameters {
  pbr_float3 base_color{1.0F, 1.0F, 1.0F};
  float metallic = 0.0F;
  float perceptual_roughness = 1.0F;
};

struct pbr_direct_light_input {
  pbr_float3 normal{0.0F, 0.0F, 1.0F};
  pbr_float3 view_direction{0.0F, 0.0F, 1.0F};
  pbr_float3 light_direction{0.0F, 0.0F, 1.0F};
  pbr_float3 radiance{1.0F, 1.0F, 1.0F};
};

inline constexpr float pbr_minimum_perceptual_roughness = 0.045F;
inline constexpr float pbr_dielectric_f0 = 0.04F;

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
