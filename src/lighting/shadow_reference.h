// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_SHADOW_REFERENCE_H
#define GRANIT_LIGHTING_SHADOW_REFERENCE_H

#include "math/math.h"

namespace granit::lighting {

struct shadow_projection {
  math::float2 uv{};
  float comparison_depth = 0.0F;
  bool inside = false;
};

/** 与 PBR Shader 相同地应用 normal bias、投影、Vulkan 深度和纹理 Y 翻转。 */
[[nodiscard]] shadow_projection
project_directional_shadow(const math::matrix4& light_view_projection, math::float3 world_position,
                           math::float3 normal, float normal_bias, float depth_bias) noexcept;

/** CPU 最近点比较参考；投影范围外按完全受光处理。 */
[[nodiscard]] float evaluate_shadow_compare(const shadow_projection& projection,
                                            float stored_depth) noexcept;

} // namespace granit::lighting

#endif
