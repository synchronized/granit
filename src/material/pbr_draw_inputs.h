// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_DRAW_INPUTS_H
#define GRANIT_MATERIAL_PBR_DRAW_INPUTS_H

#include "material/pbr_types.h"

#include <array>
#include <cstdint>

namespace granit::material {

/** 与 HLSL column-major float4x4 一致的列主序矩阵。 */
using pbr_matrix4 = math::matrix4;

struct pbr_view_input {
  pbr_matrix4 view_projection{};
  pbr_float3 camera_position{};
};

struct pbr_object_input {
  pbr_matrix4 model{};
  pbr_matrix4 normal_matrix{};
  std::uint32_t object_id = 0;
};

struct pbr_directional_light_input {
  pbr_float3 direction_to_light{0.0F, 0.0F, 1.0F};
  pbr_float3 radiance{1.0F, 1.0F, 1.0F};
};

enum class pbr_draw_input_error : std::uint8_t {
  none,
  non_finite_value,
  invalid_light_direction,
  negative_light_radiance,
};

struct alignas(16) pbr_frame_constants {
  pbr_matrix4 view_projection;
  std::array<float, 4> camera_position;
  std::array<float, 4> direction_to_light;
  std::array<float, 4> light_radiance;
  /** x 为 Specular AA 开关，其余字段保留。 */
  std::array<std::uint32_t, 4> render_options;
};

struct alignas(16) pbr_object_constants {
  pbr_matrix4 model;
  pbr_matrix4 normal_matrix;
  std::array<std::uint32_t, 4> object_id;
};

[[nodiscard]] pbr_draw_input_error
pack_pbr_draw_inputs(const pbr_view_input& view, const pbr_object_input& object,
                     const pbr_directional_light_input& light, pbr_frame_constants& frame_constants,
                     pbr_object_constants& object_constants) noexcept;

static_assert(sizeof(pbr_frame_constants) == 128);
static_assert(sizeof(pbr_object_constants) == 144);

} // namespace granit::material

#endif
