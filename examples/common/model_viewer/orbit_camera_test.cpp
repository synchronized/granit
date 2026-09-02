// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/orbit_camera.h"

#include <catch2/catch_all.hpp>

#include <cmath>

namespace {

bool close(float left, float right, float epsilon = 0.0001F) {
  return std::abs(left - right) <= epsilon;
}

} // namespace

TEST_CASE("轨道相机根据 Bounds 和宽高比自动聚焦", "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera camera;
  REQUIRE(camera.focus({.center = {1, 2, 3}, .radius = 2}, 800, 600));
  CHECK(camera.target() == granit::math::float3{1, 2, 3});
  CHECK(camera.distance() > 2.0F);
  CHECK(camera.near_plane() > 0.0F);
  CHECK(camera.far_plane() > camera.near_plane());

  granit::example::model_viewer::orbit_camera narrow;
  REQUIRE(narrow.focus({.radius = 2}, 200, 1000));
  CHECK(narrow.distance() > camera.distance());
  granit::example::model_viewer::camera_matrices matrices;
  REQUIRE(narrow.matrices(1, 8192, matrices));
  CHECK(granit::math::is_finite(matrices.view_projection));
}

TEST_CASE("轨道相机输出Vulkan裁剪空间方向", "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera camera;
  granit::example::model_viewer::camera_matrices matrices;
  REQUIRE(camera.matrices(800, 600, matrices));

  granit::math::float3 projected_up;
  REQUIRE(granit::math::transform_point(matrices.view_projection, {0, 1, 0}, projected_up));
  CHECK(matrices.projection[5] < 0.0F);
  CHECK(projected_up.y < 0.0F);
}

TEST_CASE("轨道相机限制 Pitch 和缩放距离", "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera camera;
  REQUIRE(camera.focus({.radius = 1}, 800, 600));
  const granit::example::model_viewer::viewer_input input{
      .pointer_delta_y = -1000000, .wheel_delta = 1000000, .orbiting = true};
  REQUIRE(camera.update(input, 800, 600));
  CHECK(camera.pitch() < 1.570796327F);
  CHECK(camera.distance() > camera.near_plane());
}

TEST_CASE("轨道相机使用帧缓冲高度归一化拖动", "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera first;
  granit::example::model_viewer::orbit_camera second;
  REQUIRE(
      first.update({.pointer_delta_x = 100, .pointer_delta_y = 50, .orbiting = true}, 800, 1000));
  REQUIRE(second.update({.pointer_delta_x = 200, .pointer_delta_y = 100, .orbiting = true}, 1600,
                        2000));
  CHECK(close(first.yaw(), second.yaw()));
  CHECK(close(first.pitch(), second.pitch()));
}

TEST_CASE("轨道相机尊重 UI 捕获、失焦和 Home", "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera camera;
  REQUIRE(camera.focus({.center = {2, 0, 0}, .radius = 1}, 640, 480));
  const auto home_distance = camera.distance();
  REQUIRE(
      camera.update({.pointer_delta_x = 100, .orbiting = true, .mouse_captured = true}, 640, 480));
  CHECK(camera.yaw() == 0.0F);
  REQUIRE(
      camera.update({.pointer_delta_x = 100, .orbiting = true, .window_focused = false}, 640, 480));
  CHECK(camera.yaw() == 0.0F);
  REQUIRE(camera.update({.wheel_delta = 2}, 640, 480));
  CHECK(camera.distance() != home_distance);
  REQUIRE(camera.update({.home_requested = true}, 640, 480));
  CHECK(close(camera.distance(), home_distance));
}

TEST_CASE("轨道相机零尺寸暂停且恢复时不累积移动", "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera camera;
  CHECK_FALSE(camera.update({.pointer_delta_x = 500, .orbiting = true}, 0, 0));
  CHECK(camera.yaw() == 0.0F);
  granit::example::model_viewer::camera_matrices unchanged;
  unchanged.position = {7, 8, 9};
  CHECK_FALSE(camera.matrices(0, 480, unchanged));
  CHECK(unchanged.position == granit::math::float3{7, 8, 9});
  REQUIRE(camera.update({}, 640, 480));
  CHECK(camera.yaw() == 0.0F);
}

TEST_CASE("轨道相机为空场景提供有限默认状态并支持平移后重新聚焦",
          "[example][model-viewer][camera]") {
  granit::example::model_viewer::orbit_camera camera;
  granit::example::model_viewer::camera_matrices defaults;
  REQUIRE(camera.matrices(1280, 720, defaults));
  CHECK(granit::math::is_finite(defaults.view_projection));

  REQUIRE(
      camera.update({.pointer_delta_x = 80, .pointer_delta_y = -40, .panning = true}, 1280, 720));
  CHECK(camera.target() != granit::math::float3{});
  const granit::example::model_viewer::camera_bounds bounds{.center = {4, 5, 6}, .radius = 0};
  REQUIRE(camera.update({.focus_requested = true}, 1280, 720, &bounds));
  CHECK(camera.target() == bounds.center);
  CHECK(camera.distance() > 0.0F);
  CHECK_FALSE(camera.focus({.radius = -1}, 1280, 720));
}
