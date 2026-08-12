// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/multi_view_submission.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

constexpr granit::scene::matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

granit::scene::view_input make_view(std::uint64_t layer_mask) {
  return {.view = identity,
          .projection = identity,
          .view_projection = identity,
          .camera_position = {},
          .area = {0, 0, 640, 480},
          .layer_mask = layer_mask};
}

granit::scene::renderable_input make_renderable(granit::scene::float3 center, std::uint64_t layer,
                                                std::uint32_t id) {
  return {.model = identity,
          .normal_matrix = identity,
          .bounds = {center, 0},
          .layer_mask = layer,
          .sort_key = 0,
          .payload = id,
          .object_id = id};
}

} // namespace

TEST_CASE("多 View 快照共享场景数据并生成独立可见索引") {
  std::array views{make_view(1), make_view(2)};
  std::array renderables{make_renderable({0, 0, 0.5F}, 1, 10), make_renderable({0, 0, 0.5F}, 2, 20),
                         make_renderable({2, 0, 0.5F}, 1, 30)};
  std::array directional{granit::scene::directional_light_input{{0, 0, 2}, {1, 1, 1}, 1},
                         granit::scene::directional_light_input{{0, 2, 0}, {2, 2, 2}, 2}};
  std::array points{granit::scene::point_light_input{{0, 0, 0.5F}, {1, 1, 1}, 0.1F, 1},
                    granit::scene::point_light_input{{2, 0, 0.5F}, {1, 1, 1}, 0.1F, 1},
                    granit::scene::point_light_input{{0, 0, 0.5F}, {1, 1, 1}, 0.1F, 2}};
  std::array spots{
      granit::scene::spot_light_input{{0, 0, 0.5F}, {0, 0, -1}, {1, 1, 1}, 0.1F, 0.1F, 0.2F, 2}};

  granit::scene::multi_view_snapshot snapshot;
  REQUIRE(granit::scene::build_multi_view_snapshot({.views = views,
                                                    .renderables = renderables,
                                                    .directional_lights = directional,
                                                    .point_lights = points,
                                                    .spot_lights = spots},
                                                   snapshot) ==
          granit::scene::multi_view_error::none);
  REQUIRE(snapshot.views().size() == 2);
  REQUIRE(snapshot.views()[0].renderables.indices().size() == 1);
  REQUIRE(snapshot.views()[1].renderables.indices().size() == 1);
  CHECK(snapshot.views()[0].renderables.indices().front() == 0);
  CHECK(snapshot.views()[1].renderables.indices().front() == 1);
  CHECK(snapshot.views()[0].directional_lights == std::vector<std::uint32_t>{0});
  CHECK(snapshot.views()[1].directional_lights == std::vector<std::uint32_t>{1});
  CHECK(snapshot.views()[0].point_lights == std::vector<std::uint32_t>{0});
  CHECK(snapshot.views()[1].point_lights == std::vector<std::uint32_t>{2});
  CHECK(snapshot.views()[0].spot_lights.empty());
  CHECK(snapshot.views()[1].spot_lights == std::vector<std::uint32_t>{0});

  renderables.front().payload = 999;
  CHECK(snapshot.renderables().front().payload == 10);
  CHECK(snapshot.directional_lights().front().direction_to_light == granit::scene::float3{0, 0, 1});
}

TEST_CASE("多 View 快照拒绝空 View 和非法后续 View 并保留旧结果") {
  const std::array renderables{make_renderable({0, 0, 0.5F}, 1, 10)};
  const std::array valid_views{make_view(1)};
  granit::scene::multi_view_snapshot snapshot;
  REQUIRE(granit::scene::build_multi_view_snapshot({.views = valid_views,
                                                    .renderables = renderables,
                                                    .directional_lights = {},
                                                    .point_lights = {},
                                                    .spot_lights = {}},
                                                   snapshot) ==
          granit::scene::multi_view_error::none);
  CHECK(granit::scene::build_multi_view_snapshot({}, snapshot) ==
        granit::scene::multi_view_error::empty_views);

  std::array invalid_views{make_view(1), make_view(2)};
  invalid_views[1].area.width = 0;
  CHECK(granit::scene::build_multi_view_snapshot({.views = invalid_views,
                                                  .renderables = renderables,
                                                  .directional_lights = {},
                                                  .point_lights = {},
                                                  .spot_lights = {}},
                                                 snapshot) ==
        granit::scene::multi_view_error::invalid_submission);
  REQUIRE(snapshot.views().size() == 1);
  CHECK(snapshot.views().front().renderables.indices().front() == 0);
}
