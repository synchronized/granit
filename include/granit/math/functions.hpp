// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATH_FUNCTIONS_HPP_
#define GRANIT_MATH_FUNCTIONS_HPP_

#include <granit/math/types.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>

namespace granit::math {

[[nodiscard]] inline constexpr float3 add(float3 left, float3 right) noexcept {
  return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline constexpr float3 subtract(float3 left, float3 right) noexcept {
  return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline constexpr float3 multiply(float3 value, float scalar) noexcept {
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline constexpr float dot(float3 left, float3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] inline constexpr float3 cross(float3 left, float3 right) noexcept {
  return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

[[nodiscard]] inline constexpr float length_squared(float3 value) noexcept {
  return dot(value, value);
}

[[nodiscard]] inline float length(float3 value) noexcept {
  return std::sqrt(length_squared(value));
}

/** 零向量或非有限向量返回零向量。 */
[[nodiscard]] inline float3 normalize(float3 value) noexcept {
  const auto squared = length_squared(value);
  if (!std::isfinite(squared) || squared <= 0.0F)
    return {};
  return multiply(value, 1.0F / std::sqrt(squared));
}

[[nodiscard]] inline bool is_finite(float3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] inline bool is_finite(const matrix4& value) noexcept {
  for (const auto component : value) {
    if (!std::isfinite(component))
      return false;
  }
  return true;
}

[[nodiscard]] inline constexpr matrix4 multiply(const matrix4& left,
                                                const matrix4& right) noexcept {
  matrix4 result{};
  for (std::size_t column = 0; column < 4; ++column) {
    for (std::size_t row = 0; row < 4; ++row) {
      for (std::size_t index = 0; index < 4; ++index)
        result[column * 4 + row] += left[index * 4 + row] * right[column * 4 + index];
    }
  }
  return result;
}

[[nodiscard]] inline constexpr float4 transform(const matrix4& matrix, float4 value) noexcept {
  return {matrix[0] * value.x + matrix[4] * value.y + matrix[8] * value.z + matrix[12] * value.w,
          matrix[1] * value.x + matrix[5] * value.y + matrix[9] * value.z + matrix[13] * value.w,
          matrix[2] * value.x + matrix[6] * value.y + matrix[10] * value.z + matrix[14] * value.w,
          matrix[3] * value.x + matrix[7] * value.y + matrix[11] * value.z + matrix[15] * value.w};
}

[[nodiscard]] inline constexpr float3 transform_vector(const matrix4& matrix,
                                                       float3 value) noexcept {
  const auto result = transform(matrix, {value.x, value.y, value.z, 0.0F});
  return {result.x, result.y, result.z};
}

/** w 为零或结果非有限时返回 false，且不修改 output。 */
[[nodiscard]] inline bool transform_point(const matrix4& matrix, float3 value,
                                          float3& output) noexcept {
  const auto result = transform(matrix, {value.x, value.y, value.z, 1.0F});
  if (!std::isfinite(result.w) || result.w == 0.0F)
    return false;
  const auto candidate = float3{result.x / result.w, result.y / result.w, result.z / result.w};
  if (!is_finite(candidate))
    return false;
  output = candidate;
  return true;
}

[[nodiscard]] inline constexpr matrix4 translation_matrix4(float3 value) noexcept {
  auto result = identity_matrix4;
  result[12] = value.x;
  result[13] = value.y;
  result[14] = value.z;
  return result;
}

[[nodiscard]] inline constexpr matrix4 scaling_matrix4(float3 value) noexcept {
  matrix4 result{};
  result[0] = value.x;
  result[5] = value.y;
  result[10] = value.z;
  result[15] = 1.0F;
  return result;
}

[[nodiscard]] inline matrix4 rotation_x_matrix4(float radians) noexcept {
  const auto cosine = std::cos(radians);
  const auto sine = std::sin(radians);
  return {{1, 0, 0, 0, 0, cosine, sine, 0, 0, -sine, cosine, 0, 0, 0, 0, 1}};
}

[[nodiscard]] inline matrix4 rotation_y_matrix4(float radians) noexcept {
  const auto cosine = std::cos(radians);
  const auto sine = std::sin(radians);
  return {{cosine, 0, -sine, 0, 0, 1, 0, 0, sine, 0, cosine, 0, 0, 0, 0, 1}};
}

[[nodiscard]] inline matrix4 rotation_z_matrix4(float radians) noexcept {
  const auto cosine = std::cos(radians);
  const auto sine = std::sin(radians);
  return {{cosine, sine, 0, 0, -sine, cosine, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
}

/** 构造右手 View 矩阵；退化方向或 up 时返回 false，且不修改 output。 */
[[nodiscard]] inline bool look_at_rh(float3 eye, float3 target, float3 up,
                                     matrix4& output) noexcept {
  if (!is_finite(eye) || !is_finite(target) || !is_finite(up))
    return false;
  const auto forward = normalize(subtract(target, eye));
  const auto side = normalize(cross(forward, up));
  if (length_squared(forward) == 0.0F || length_squared(side) == 0.0F)
    return false;
  const auto corrected_up = cross(side, forward);
  const matrix4 candidate{side.x,          corrected_up.x,          -forward.x,        0.0F,
                          side.y,          corrected_up.y,          -forward.y,        0.0F,
                          side.z,          corrected_up.z,          -forward.z,        0.0F,
                          -dot(side, eye), -dot(corrected_up, eye), dot(forward, eye), 1.0F};
  output = candidate;
  return true;
}

/** 构造右手、深度范围 [0,1] 的透视投影；非法参数不修改 output。 */
[[nodiscard]] inline bool perspective_rh_zo(float vertical_fov_radians, float aspect,
                                            float near_plane, float far_plane,
                                            matrix4& output) noexcept {
  if (!std::isfinite(vertical_fov_radians) || !std::isfinite(aspect) ||
      !std::isfinite(near_plane) || !std::isfinite(far_plane) ||
      !(vertical_fov_radians > 0.0F && vertical_fov_radians < std::numbers::pi_v<float>) ||
      !(aspect > 0.0F) || !(near_plane > 0.0F) || !(far_plane > near_plane))
    return false;
  const auto vertical_scale = 1.0F / std::tan(vertical_fov_radians * 0.5F);
  const auto inverse_depth = 1.0F / (near_plane - far_plane);
  output = {vertical_scale / aspect,
            0,
            0,
            0,
            0,
            vertical_scale,
            0,
            0,
            0,
            0,
            far_plane * inverse_depth,
            -1,
            0,
            0,
            far_plane * near_plane * inverse_depth,
            0};
  return true;
}

/** 构造右手、深度范围 [0,1] 的正交投影；非法参数不修改 output。 */
[[nodiscard]] inline bool orthographic_rh_zo(float left, float right, float bottom, float top,
                                             float near_plane, float far_plane,
                                             matrix4& output) noexcept {
  if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(bottom) ||
      !std::isfinite(top) || !std::isfinite(near_plane) || !std::isfinite(far_plane) ||
      !(right > left) || !(top > bottom) || !(near_plane >= 0.0F) || !(far_plane > near_plane))
    return false;
  output = {2.0F / (right - left),
            0,
            0,
            0,
            0,
            2.0F / (top - bottom),
            0,
            0,
            0,
            0,
            1.0F / (near_plane - far_plane),
            0,
            -(right + left) / (right - left),
            -(top + bottom) / (top - bottom),
            near_plane / (near_plane - far_plane),
            1};
  return true;
}

} // namespace granit::math

#endif
