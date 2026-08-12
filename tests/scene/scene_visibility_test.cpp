// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/scene_visibility.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

constexpr granit::scene::matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

granit::scene::renderable_input make_renderable(granit::scene::float3 center, float radius,
                                                std::uint64_t layer, std::uint64_t sort_key,
                                                std::uint32_t object_id) {
  return {.model = identity,
          .normal_matrix = identity,
          .bounds = {center, radius},
          .layer_mask = layer,
          .sort_key = sort_key,
          .payload = object_id + 100,
          .object_id = object_id};
}

granit::scene::frame_submission
make_submission(std::span<const granit::scene::renderable_input> renderables) {
  return {.view = {.view = identity,
                   .projection = identity,
                   .view_projection = identity,
                   .camera_position = {},
                   .area = {0, 0, 640, 480},
                   .layer_mask = 1},
          .renderables = renderables,
          .directional_lights = {},
          .point_lights = {},
          .spot_lights = {}};
}

} // namespace

TEST_CASE("Vulkan Frustum 使用负一到一 XY 与零到一 Z") {
  granit::scene::frustum value{};
  REQUIRE(granit::scene::extract_frustum(identity, value) == granit::scene::visibility_error::none);
  CHECK(granit::scene::intersects(value, {{0, 0, 0.5F}, 0}));
  CHECK(granit::scene::intersects(value, {{1.5F, 0, 0.5F}, 0.5F}));
  CHECK_FALSE(granit::scene::intersects(value, {{1.51F, 0, 0.5F}, 0.5F}));
  CHECK_FALSE(granit::scene::intersects(value, {{0, 0, -0.01F}, 0}));
  CHECK_FALSE(granit::scene::intersects(value, {{0, 0, 1.01F}, 0}));
}

TEST_CASE("可见列表执行层过滤并按稳定键排序") {
  const std::array renderables{
      make_renderable({0, 0, 0.5F}, 0, 1, 2, 9), make_renderable({0, 0, 0.5F}, 0, 1, 1, 7),
      make_renderable({0, 0, 0.5F}, 0, 1, 1, 3), make_renderable({0, 0, 0.5F}, 0, 1, 1, 3),
      make_renderable({0, 0, 0.5F}, 0, 2, 0, 1), make_renderable({2, 0, 0.5F}, 0, 1, 0, 2),
  };
  granit::scene::frame_snapshot snapshot;
  REQUIRE(granit::scene::build_frame_snapshot(make_submission(renderables), snapshot) ==
          granit::scene::submission_error::none);
  granit::scene::visible_list visible;
  REQUIRE(granit::scene::build_visible_list(snapshot, visible) ==
          granit::scene::visibility_error::none);
  REQUIRE(visible.indices().size() == 4);
  CHECK(visible.indices()[0] == 2);
  CHECK(visible.indices()[1] == 3);
  CHECK(visible.indices()[2] == 1);
  CHECK(visible.indices()[3] == 0);
}

TEST_CASE("非法 Frustum 不覆盖已有可见列表") {
  const std::array renderables{make_renderable({0, 0, 0.5F}, 0, 1, 0, 1)};
  granit::scene::frame_snapshot snapshot;
  REQUIRE(granit::scene::build_frame_snapshot(make_submission(renderables), snapshot) ==
          granit::scene::submission_error::none);
  granit::scene::visible_list visible;
  REQUIRE(granit::scene::build_visible_list(snapshot, visible) ==
          granit::scene::visibility_error::none);
  REQUIRE(visible.indices().size() == 1);

  auto invalid_submission = make_submission(renderables);
  invalid_submission.view.view_projection = {};
  granit::scene::frame_snapshot invalid_snapshot;
  REQUIRE(granit::scene::build_frame_snapshot(invalid_submission, invalid_snapshot) ==
          granit::scene::submission_error::none);
  CHECK(granit::scene::build_visible_list(invalid_snapshot, visible) ==
        granit::scene::visibility_error::invalid_frustum);
  REQUIRE(visible.indices().size() == 1);
  CHECK(visible.indices().front() == 0);
}
