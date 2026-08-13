// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/math/types.hpp>

#include <type_traits>

static_assert(std::is_same_v<granit::math::float3, granit_float3>);
static_assert(std::is_same_v<granit::math::matrix4, granit_matrix4>);
static_assert(std::is_standard_layout_v<granit::math::float3>);
static_assert(std::is_trivially_copyable_v<granit::math::float3>);
static_assert(std::is_standard_layout_v<granit::math::matrix4>);
static_assert(std::is_trivially_copyable_v<granit::math::matrix4>);
static_assert(std::is_aggregate_v<granit::math::matrix4>);
static_assert(granit::math::identity_matrix4[0] == 1.0F);
