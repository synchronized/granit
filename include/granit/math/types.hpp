// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATH_TYPES_HPP_
#define GRANIT_MATH_TYPES_HPP_

#include <granit/math/types.h>

namespace granit::math {

using float2 = ::granit_float2;
using float3 = ::granit_float3;
using float4 = ::granit_float4;
using matrix4 = ::granit_matrix4;

inline constexpr matrix4 identity_matrix4{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

} // namespace granit::math

#endif
