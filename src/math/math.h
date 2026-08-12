// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATH_MATH_H
#define GRANIT_MATH_MATH_H

#include <array>

namespace granit::math {

struct float2 {
  float x = 0.0F;
  float y = 0.0F;

  friend bool operator==(const float2&, const float2&) = default;
};

struct float3 {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;

  friend bool operator==(const float3&, const float3&) = default;
};

struct float4 {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float w = 0.0F;

  friend bool operator==(const float4&, const float4&) = default;
};

/** 与 HLSL column-major float4x4 一致的列主序矩阵。 */
using matrix4 = std::array<float, 16>;

inline constexpr matrix4 identity_matrix4{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

[[nodiscard]] float3 add(float3 left, float3 right) noexcept;
[[nodiscard]] float3 subtract(float3 left, float3 right) noexcept;
[[nodiscard]] float3 multiply(float3 value, float scalar) noexcept;
[[nodiscard]] float dot(float3 left, float3 right) noexcept;
[[nodiscard]] float3 cross(float3 left, float3 right) noexcept;
[[nodiscard]] float length_squared(float3 value) noexcept;
[[nodiscard]] float length(float3 value) noexcept;
/** 零向量或非有限向量返回零向量。 */
[[nodiscard]] float3 normalize(float3 value) noexcept;
[[nodiscard]] bool is_finite(float3 value) noexcept;
[[nodiscard]] bool is_finite(const matrix4& value) noexcept;

[[nodiscard]] matrix4 multiply(const matrix4& left, const matrix4& right) noexcept;
[[nodiscard]] float4 transform(const matrix4& matrix, float4 value) noexcept;
[[nodiscard]] float3 transform_vector(const matrix4& matrix, float3 value) noexcept;
/** w 为零或结果非有限时返回 false。 */
[[nodiscard]] bool transform_point(const matrix4& matrix, float3 value, float3& output) noexcept;

/** 构造右手 View 矩阵；退化方向或 up 时返回 false 且不修改 output。 */
[[nodiscard]] bool look_at_rh(float3 eye, float3 target, float3 up, matrix4& output) noexcept;

/** 构造右手、深度范围 [0,1] 的透视投影；非法参数不修改 output。 */
[[nodiscard]] bool perspective_rh_zo(float vertical_fov_radians, float aspect, float near_plane,
                                     float far_plane, matrix4& output) noexcept;

/** 构造右手、深度范围 [0,1] 的正交投影；非法参数不修改 output。 */
[[nodiscard]] bool orthographic_rh_zo(float left, float right, float bottom, float top,
                                      float near_plane, float far_plane, matrix4& output) noexcept;

} // namespace granit::math

#endif
