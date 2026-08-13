// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATH_TYPES_H_
#define GRANIT_MATH_TYPES_H_

#include <stddef.h>

/** 跨 C ABI 传递的二维浮点值，不附带大型数学库。 */
typedef struct granit_float2 {
  float x;
  float y;
#ifdef __cplusplus
  friend bool operator==(const granit_float2&, const granit_float2&) = default;
#endif
} granit_float2;

/** 跨 C ABI 传递的三维浮点值，不附带大型数学库。 */
typedef struct granit_float3 {
  float x;
  float y;
  float z;
#ifdef __cplusplus
  friend bool operator==(const granit_float3&, const granit_float3&) = default;
#endif
} granit_float3;

/** 跨 C ABI 传递的四维浮点值，不附带大型数学库。 */
typedef struct granit_float4 {
  float x;
  float y;
  float z;
  float w;
#ifdef __cplusplus
  friend bool operator==(const granit_float4&, const granit_float4&) = default;
#endif
} granit_float4;

/** 与 HLSL column-major float4x4 一致的列主序矩阵。 */
typedef struct granit_matrix4 {
  float elements[16];
#ifdef __cplusplus
  constexpr float* begin() noexcept { return elements; }
  constexpr const float* begin() const noexcept { return elements; }
  constexpr float* end() noexcept { return elements + 16; }
  constexpr const float* end() const noexcept { return elements + 16; }
  constexpr size_t size() const noexcept { return 16; }
  constexpr float& operator[](size_t index) noexcept { return elements[index]; }
  constexpr const float& operator[](size_t index) const noexcept { return elements[index]; }
  friend bool operator==(const granit_matrix4&, const granit_matrix4&) = default;
#endif
} granit_matrix4;

#endif
