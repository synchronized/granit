// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_reference.h"

#include <catch2/catch_all.hpp>

TEST_CASE("阴影投影匹配Vulkan深度和纹理Y方向") {
  const auto projection = granit::lighting::project_directional_shadow(
      granit::math::identity_matrix4, {0.5F, -0.5F, 0.6F}, {0, 0, 1}, 0.1F, 0.02F);
  REQUIRE(projection.inside);
  CHECK(projection.uv.x == Catch::Approx(0.75F));
  CHECK(projection.uv.y == Catch::Approx(0.75F));
  CHECK(projection.comparison_depth == Catch::Approx(0.68F));
  CHECK(granit::lighting::evaluate_shadow_compare(projection, 0.7F) == 1.0F);
  CHECK(granit::lighting::evaluate_shadow_compare(projection, 0.5F) == 0.0F);
}

TEST_CASE("阴影范围外完全受光且非法输入安全失败") {
  const auto outside = granit::lighting::project_directional_shadow(granit::math::identity_matrix4,
                                                                    {2, 0, 0.5F}, {0, 0, 1}, 0, 0);
  CHECK_FALSE(outside.inside);
  CHECK(granit::lighting::evaluate_shadow_compare(outside, 0.0F) == 1.0F);

  const auto invalid =
      granit::lighting::project_directional_shadow(granit::math::identity_matrix4, {}, {}, -1, 0);
  CHECK_FALSE(invalid.inside);
}
