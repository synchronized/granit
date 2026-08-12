// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/scene_submission.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <limits>

namespace {

constexpr granit::scene::matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

granit::scene::view_input make_view() {
  return {.view = identity,
          .projection = identity,
          .view_projection = identity,
          .camera_position = {0, 0, 2},
          .area = {0, 0, 1280, 720},
          .layer_mask = 3};
}

granit::scene::frame_submission make_submission() {
  return {.view = make_view(),
          .renderables = {},
          .directional_lights = {},
          .point_lights = {},
          .spot_lights = {}};
}

} // namespace

TEST_CASE("场景帧快照复制输入并规范化光源方向") {
  std::array renderables{granit::scene::renderable_input{.model = identity,
                                                         .normal_matrix = identity,
                                                         .bounds = {{1, 2, 3}, 4},
                                                         .layer_mask = 1,
                                                         .sort_key = 8,
                                                         .payload = 42,
                                                         .object_id = 7}};
  std::array directional{granit::scene::directional_light_input{
      .direction_to_light = {0, 0, 2}, .radiance = {3, 2, 1}, .layer_mask = 1}};
  std::array points{granit::scene::point_light_input{
      .position = {1, 2, 3}, .intensity = {4, 5, 6}, .radius = 10, .layer_mask = 2}};
  std::array spots{granit::scene::spot_light_input{.position = {3, 2, 1},
                                                   .direction = {0, -2, 0},
                                                   .intensity = {1, 2, 3},
                                                   .radius = 8,
                                                   .inner_angle = 0.2F,
                                                   .outer_angle = 0.4F,
                                                   .layer_mask = 3}};
  granit::scene::frame_snapshot snapshot;
  REQUIRE(granit::scene::build_frame_snapshot({.view = make_view(),
                                               .renderables = renderables,
                                               .directional_lights = directional,
                                               .point_lights = points,
                                               .spot_lights = spots},
                                              snapshot) == granit::scene::submission_error::none);

  renderables.front().payload = 99;
  directional.front().radiance = {};
  CHECK(snapshot.renderables().front().payload == 42);
  CHECK(snapshot.directional_lights().front().radiance == granit::scene::float3{3, 2, 1});
  CHECK(snapshot.directional_lights().front().direction_to_light == granit::scene::float3{0, 0, 1});
  CHECK(snapshot.spot_lights().front().direction == granit::scene::float3{0, -1, 0});
  CHECK(snapshot.point_lights().front().radius == 10);
  CHECK(snapshot.view().area.width == 1280);
}

TEST_CASE("场景帧快照拒绝非法 View 与 Renderable") {
  granit::scene::frame_snapshot snapshot;
  auto view = make_view();
  view.area.width = 0;
  auto submission = make_submission();
  submission.view = view;
  CHECK(granit::scene::build_frame_snapshot(submission, snapshot) ==
        granit::scene::submission_error::invalid_viewport);
  view = make_view();
  view.view[0] = std::numeric_limits<float>::infinity();
  submission.view = view;
  CHECK(granit::scene::build_frame_snapshot(submission, snapshot) ==
        granit::scene::submission_error::non_finite_value);
  const granit::scene::renderable_input invalid{.model = identity,
                                                .normal_matrix = identity,
                                                .bounds = {{}, -1},
                                                .layer_mask = UINT64_MAX,
                                                .sort_key = 0,
                                                .payload = 0,
                                                .object_id = 0};
  view = make_view();
  submission.view = view;
  submission.renderables = std::span{&invalid, 1};
  CHECK(granit::scene::build_frame_snapshot(submission, snapshot) ==
        granit::scene::submission_error::invalid_bounds);
}

TEST_CASE("场景帧快照拒绝非法光源并保持旧快照") {
  granit::scene::frame_snapshot snapshot;
  const std::array initial{granit::scene::renderable_input{.model = identity,
                                                           .normal_matrix = identity,
                                                           .bounds = {{}, 1},
                                                           .layer_mask = UINT64_MAX,
                                                           .sort_key = 0,
                                                           .payload = 123,
                                                           .object_id = 0}};
  auto submission = make_submission();
  submission.renderables = initial;
  REQUIRE(granit::scene::build_frame_snapshot(submission, snapshot) ==
          granit::scene::submission_error::none);

  const std::array invalid_direction{granit::scene::directional_light_input{
      .direction_to_light = {}, .radiance = {1, 1, 1}, .layer_mask = UINT64_MAX}};
  submission = make_submission();
  submission.directional_lights = invalid_direction;
  CHECK(granit::scene::build_frame_snapshot(submission, snapshot) ==
        granit::scene::submission_error::invalid_direction);
  CHECK(snapshot.renderables().front().payload == 123);

  const std::array invalid_point{granit::scene::point_light_input{
      .position = {}, .intensity = {1, 1, 1}, .radius = 0, .layer_mask = UINT64_MAX}};
  submission = make_submission();
  submission.point_lights = invalid_point;
  CHECK(granit::scene::build_frame_snapshot(submission, snapshot) ==
        granit::scene::submission_error::invalid_light_radius);
  const std::array invalid_spot{granit::scene::spot_light_input{.position = {},
                                                                .direction = {0, 0, -1},
                                                                .intensity = {1, 1, 1},
                                                                .radius = 1,
                                                                .inner_angle = 0.5F,
                                                                .outer_angle = 0.4F,
                                                                .layer_mask = UINT64_MAX}};
  submission = make_submission();
  submission.spot_lights = invalid_spot;
  CHECK(granit::scene::build_frame_snapshot(submission, snapshot) ==
        granit::scene::submission_error::invalid_spot_cone);
}
