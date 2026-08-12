// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_data.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <numbers>

namespace {

granit::scene::multi_view_snapshot make_snapshot() {
  granit::scene::view_input view{};
  view.view = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  view.projection = view.view;
  view.view_projection = view.view;
  const std::array directions{granit::scene::directional_light_input{{0, 0, 1}, {1, 2, 3}}};
  const std::array points{granit::scene::point_light_input{{0, 0, 0.5F}, {4, 5, 6}, 2}};
  const std::array spots{granit::scene::spot_light_input{
      {0, 0, 0.5F}, {0, 0, -1}, {7, 8, 9}, 3, 0.25F, std::numbers::pi_v<float> / 3.0F}};
  granit::scene::multi_view_snapshot result;
  REQUIRE(granit::scene::build_multi_view_snapshot({.views = std::span{&view, 1},
                                                    .renderables = {},
                                                    .directional_lights = directions,
                                                    .point_lights = points,
                                                    .spot_lights = spots},
                                                   result) ==
          granit::scene::multi_view_error::none);
  return result;
}

} // namespace

TEST_CASE("逐View光源转换为稳定GPU布局") {
  const auto snapshot = make_snapshot();
  granit::lighting::packed_view_lights packed;
  granit::lighting::light_requirements required;
  REQUIRE(granit::lighting::pack_view_lights(snapshot, 0, {}, packed, required) ==
          granit::lighting::light_pack_error::none);
  CHECK(required.directional == 1);
  CHECK(required.point == 1);
  CHECK(required.spot == 1);
  REQUIRE(packed.directional.size() == 1);
  REQUIRE(packed.point.size() == 1);
  REQUIRE(packed.spot.size() == 1);
  CHECK(packed.directional[0].radiance[1] == 2.0F);
  CHECK(packed.point[0].radius == 2.0F);
  CHECK(packed.spot[0].intensity[2] == 9.0F);
  CHECK(packed.spot[0].outer_angle_cosine == Catch::Approx(0.5F));
}

TEST_CASE("容量失败报告需求且保留旧结果") {
  const auto snapshot = make_snapshot();
  granit::lighting::packed_view_lights packed;
  packed.point.push_back({});
  packed.point[0].radius = 99.0F;
  granit::lighting::light_requirements required;
  CHECK(granit::lighting::pack_view_lights(snapshot, 0, {.directional = 0, .point = 0, .spot = 0},
                                           packed, required) ==
        granit::lighting::light_pack_error::capacity_exceeded);
  CHECK(required.directional == 1);
  CHECK(required.point == 1);
  CHECK(required.spot == 1);
  REQUIRE(packed.point.size() == 1);
  CHECK(packed.point[0].radius == 99.0F);
}

TEST_CASE("拒绝非法容量和View索引") {
  const auto snapshot = make_snapshot();
  granit::lighting::packed_view_lights packed;
  granit::lighting::light_requirements required;
  CHECK(granit::lighting::pack_view_lights(
            snapshot, 0, {.directional = granit::lighting::maximum_directional_lights + 1}, packed,
            required) == granit::lighting::light_pack_error::invalid_limits);
  CHECK(granit::lighting::pack_view_lights(snapshot, 1, {}, packed, required) ==
        granit::lighting::light_pack_error::view_out_of_range);
}
