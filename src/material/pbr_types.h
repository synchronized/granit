// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_TYPES_H
#define GRANIT_MATERIAL_PBR_TYPES_H

#include "math/math.h"

namespace granit::material {

using pbr_float3 = math::float3;

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

} // namespace granit::material

#endif
