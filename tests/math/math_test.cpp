// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "math/math.h"

#include <catch2/catch_all.hpp>

#include <limits>
#include <numbers>

namespace {

void check_float3(granit::math::float3 actual, granit::math::float3 expected) {
  CHECK(actual.x == Catch::Approx(expected.x).margin(0.00001F));
  CHECK(actual.y == Catch::Approx(expected.y).margin(0.00001F));
  CHECK(actual.z == Catch::Approx(expected.z).margin(0.00001F));
}

} // namespace

TEST_CASE("向量运算使用右手坐标约定") {
  using namespace granit::math;
  CHECK(dot({1, 2, 3}, {4, 5, 6}) == 32.0F);
  CHECK(cross({1, 0, 0}, {0, 1, 0}) == float3{0, 0, 1});
  check_float3(normalize({0, 0, 2}), {0, 0, 1});
  CHECK(normalize({}) == float3{});
}

TEST_CASE("列主序矩阵乘法和点变换保持顺序") {
  auto translation = granit::math::identity_matrix4;
  translation[12] = 2.0F;
  translation[13] = 3.0F;
  translation[14] = 4.0F;
  auto scale = granit::math::identity_matrix4;
  scale[0] = 2.0F;
  scale[5] = 3.0F;
  scale[10] = 4.0F;
  const auto combined = granit::math::multiply(translation, scale);
  granit::math::float3 result;
  REQUIRE(granit::math::transform_point(combined, {1, 1, 1}, result));
  check_float3(result, {4, 6, 8});
}

TEST_CASE("右手View矩阵将观察目标置于负Z轴") {
  auto view = granit::math::identity_matrix4;
  REQUIRE(granit::math::look_at_rh({0, 0, 1}, {0, 0, 0}, {0, 1, 0}, view));
  granit::math::float3 target;
  REQUIRE(granit::math::transform_point(view, {0, 0, 0}, target));
  check_float3(target, {0, 0, -1});

  const auto previous = view;
  CHECK_FALSE(granit::math::look_at_rh({}, {}, {0, 1, 0}, view));
  CHECK(view == previous);
}

TEST_CASE("透视投影使用Vulkan零到一深度") {
  granit::math::matrix4 projection{};
  REQUIRE(granit::math::perspective_rh_zo(std::numbers::pi_v<float> / 2.0F, 1.0F, 1.0F, 10.0F,
                                          projection));
  granit::math::float3 near_point;
  granit::math::float3 far_point;
  REQUIRE(granit::math::transform_point(projection, {0, 0, -1}, near_point));
  REQUIRE(granit::math::transform_point(projection, {0, 0, -10}, far_point));
  CHECK(near_point.z == Catch::Approx(0.0F).margin(0.00001F));
  CHECK(far_point.z == Catch::Approx(1.0F).margin(0.00001F));
}

TEST_CASE("正交投影使用Vulkan零到一深度并拒绝非法参数") {
  granit::math::matrix4 projection{};
  REQUIRE(granit::math::orthographic_rh_zo(-2, 2, -1, 1, 1, 11, projection));
  granit::math::float3 near_point;
  granit::math::float3 far_point;
  REQUIRE(granit::math::transform_point(projection, {-2, -1, -1}, near_point));
  REQUIRE(granit::math::transform_point(projection, {2, 1, -11}, far_point));
  check_float3(near_point, {-1, -1, 0});
  check_float3(far_point, {1, 1, 1});

  const auto previous = projection;
  CHECK_FALSE(granit::math::orthographic_rh_zo(1, 1, -1, 1, 1, 11, projection));
  CHECK(projection == previous);
  CHECK_FALSE(granit::math::perspective_rh_zo(std::numeric_limits<float>::infinity(), 1, 1, 10,
                                              projection));
}
