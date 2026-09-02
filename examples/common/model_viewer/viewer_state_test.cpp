// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_state.h"

#include <catch2/catch_all.hpp>

#include <limits>

using namespace granit::example;

TEST_CASE("查看器状态集中管理选择和可见性", "[example][model-viewer][state]") {
  gltf::scene scene;
  scene.nodes.resize(2);
  scene.materials.resize(1);
  model_viewer::viewer_state state;
  state.reset(scene);
  model_viewer::viewer_change change;
  change.selected_node = 1U;
  change.selected_material = 0U;
  change.visibility_node = 0U;
  change.visible = false;
  REQUIRE(state.apply(scene, change) == model_viewer::viewer_state_error::none);
  CHECK(state.selected_node() == 1U);
  CHECK(state.selected_material() == 0U);
  CHECK_FALSE(state.node_visible(0));
  CHECK(state.node_visible(1));
  CHECK(state.background_color() == granit::math::float3{0.025F, 0.04F, 0.065F});
  CHECK(state.directional_light().direction == granit::math::float3{0.0F, -1.0F, 1.0F});
  CHECK(state.environment_intensity() == 0.15F);
}

TEST_CASE("查看器状态拒绝无效批次且保留旧状态", "[example][model-viewer][state]") {
  gltf::scene scene;
  scene.nodes.resize(1);
  model_viewer::viewer_state state;
  state.reset(scene);
  model_viewer::viewer_change valid;
  valid.exposure_ev = 2.0F;
  valid.environment_intensity = 1.5F;
  valid.environment_rotation_radians = 0.75F;
  REQUIRE(state.apply(scene, valid) == model_viewer::viewer_state_error::none);

  model_viewer::viewer_change invalid_batch;
  invalid_batch.visibility_node = 0U;
  invalid_batch.visible = false;
  invalid_batch.exposure_ev = std::numeric_limits<float>::infinity();
  CHECK(state.apply(scene, invalid_batch) == model_viewer::viewer_state_error::invalid_exposure);
  CHECK(state.node_visible(0));
  CHECK(state.exposure_ev() == 2.0F);
  CHECK(state.environment_intensity() == 1.5F);
  CHECK(state.environment_rotation_radians() == 0.75F);

  model_viewer::viewer_change invalid_background;
  invalid_background.background_color = {0.1F, -0.1F, 0.2F};
  CHECK(state.apply(scene, invalid_background) ==
        model_viewer::viewer_state_error::invalid_background);

  model_viewer::viewer_change invalid_environment;
  invalid_environment.environment_intensity = -1.0F;
  CHECK(state.apply(scene, invalid_environment) ==
        model_viewer::viewer_state_error::invalid_environment);

  model_viewer::viewer_change invalid_selection;
  invalid_selection.selected_node = 4U;
  CHECK(state.apply(scene, invalid_selection) ==
        model_viewer::viewer_state_error::invalid_selection);
}

TEST_CASE("查看器状态校验灯光、调试模式和成对可见性参数", "[example][model-viewer][state]") {
  gltf::scene scene;
  scene.nodes.resize(1);
  model_viewer::viewer_state state;
  state.reset(scene);

  model_viewer::viewer_change incomplete_visibility;
  incomplete_visibility.visibility_node = 0U;
  CHECK(state.apply(scene, incomplete_visibility) ==
        model_viewer::viewer_state_error::invalid_visibility_change);

  model_viewer::viewer_change invalid_light;
  invalid_light.directional_light =
      model_viewer::directional_light_state{.direction = {}, .radiance = {1, 1, 1}};
  CHECK(state.apply(scene, invalid_light) == model_viewer::viewer_state_error::invalid_light);

  model_viewer::viewer_change invalid_debug;
  invalid_debug.debug_display = static_cast<model_viewer::debug_display_mode>(100);
  CHECK(state.apply(scene, invalid_debug) ==
        model_viewer::viewer_state_error::invalid_debug_display);
}

TEST_CASE("查看器状态在 Scene 替换后收敛旧引用", "[example][model-viewer][state]") {
  gltf::scene first;
  first.nodes.resize(2);
  first.materials.resize(2);
  model_viewer::viewer_state state;
  state.reset(first);
  model_viewer::viewer_change selection;
  selection.selected_node = 1U;
  selection.selected_material = 1U;
  REQUIRE(state.apply(first, selection) == model_viewer::viewer_state_error::none);

  gltf::scene second;
  second.nodes.resize(1);
  REQUIRE(state.apply(second, {}) == model_viewer::viewer_state_error::none);
  CHECK(state.selected_node() == gltf::invalid_index);
  CHECK(state.selected_material() == gltf::invalid_index);
  CHECK(state.node_visibility().size() == 1);
  CHECK(state.node_visible(0));
}
