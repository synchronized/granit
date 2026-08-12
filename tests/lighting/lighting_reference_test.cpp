// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/lighting_reference.h"

#include <catch2/catch_all.hpp>

#include <numbers>

TEST_CASE("距离衰减在半径处平滑归零") {
  CHECK(granit::lighting::distance_attenuation(0.0F, 10.0F) == 10'000.0F);
  CHECK(granit::lighting::distance_attenuation(100.0F, 10.0F) == 0.0F);
  CHECK(granit::lighting::distance_attenuation(101.0F, 10.0F) == 0.0F);
  CHECK(granit::lighting::distance_attenuation(1.0F, 0.0F) == 0.0F);
}

TEST_CASE("聚光锥响应覆盖内锥过渡和外部") {
  using granit::material::pbr_float3;
  constexpr auto inner = std::numbers::pi_v<float> / 6.0F;
  constexpr auto outer = std::numbers::pi_v<float> / 3.0F;
  CHECK(granit::lighting::spot_angle_attenuation({0, 0, -1}, {0, 0, -1}, inner, outer) == 1.0F);
  CHECK(granit::lighting::spot_angle_attenuation({1, 0, 0}, {0, 0, -1}, inner, outer) == 0.0F);
  const auto transition = granit::lighting::spot_angle_attenuation(
      pbr_float3{0.70710678F, 0, -0.70710678F}, {0, 0, -1}, inner, outer);
  CHECK(transition > 0.0F);
  CHECK(transition < 1.0F);
}

TEST_CASE("点光和聚光使用相同PBR直接光参考") {
  granit::lighting::lighting_surface surface{};
  const granit::scene::point_light_input point{{0, 0, 1}, {1, 1, 1}, 2};
  const auto point_result = granit::lighting::evaluate_point_light(surface, point);
  CHECK(point_result.x > 0.0F);
  CHECK(point_result.y > 0.0F);
  CHECK(point_result.z > 0.0F);

  const granit::scene::spot_light_input spot{
      {0, 0, 1}, {0, 0, -1}, {1, 1, 1}, 2, 0.0F, std::numbers::pi_v<float> / 4.0F};
  CHECK(granit::lighting::evaluate_spot_light(surface, spot) == point_result);

  auto outside = surface;
  outside.position = {2, 0, 0};
  CHECK(granit::lighting::evaluate_point_light(outside, point) == granit::material::pbr_float3{});
  CHECK(granit::lighting::evaluate_spot_light(outside, spot) == granit::material::pbr_float3{});
}
