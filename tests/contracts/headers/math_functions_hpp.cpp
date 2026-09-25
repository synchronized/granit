// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/math/functions.hpp>

static_assert(granit::math::dot({1, 2, 3}, {4, 5, 6}) == 32.0F);
static_assert(granit::math::cross({1, 0, 0}, {0, 1, 0}) == granit::math::float3{0, 0, 1});
static_assert(granit::math::translation_matrix4({1, 2, 3})[12] == 1.0F);
static_assert(granit::math::scaling_matrix4({2, 3, 4})[10] == 4.0F);
