// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/scene_pbr_adapter.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

constexpr granit::scene::matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

granit::scene::multi_view_snapshot make_snapshot(std::size_t directional_light_count = 1) {
  const std::array views{granit::scene::view_input{.view = identity,
                                                   .projection = identity,
                                                   .view_projection = identity,
                                                   .camera_position = {0, 0, 2},
                                                   .area = {0, 0, 640, 480},
                                                   .layer_mask = 1}};
  const std::array renderables{granit::scene::renderable_input{.model = identity,
                                                               .normal_matrix = identity,
                                                               .bounds = {{0, 0, 0.5F}, 0},
                                                               .layer_mask = 1,
                                                               .sort_key = 2,
                                                               .payload = 200,
                                                               .object_id = 20},
                               granit::scene::renderable_input{.model = identity,
                                                               .normal_matrix = identity,
                                                               .bounds = {{0, 0, 0.5F}, 0},
                                                               .layer_mask = 1,
                                                               .sort_key = 1,
                                                               .payload = 100,
                                                               .object_id = 10}};
  std::array directional{granit::scene::directional_light_input{{0, 0, 2}, {3, 2, 1}, 1},
                         granit::scene::directional_light_input{{0, 2, 0}, {1, 1, 1}, 1}};
  granit::scene::multi_view_snapshot snapshot;
  const auto result = granit::scene::build_multi_view_snapshot(
      {.views = views,
       .renderables = renderables,
       .directional_lights = std::span{directional}.first(directional_light_count),
       .point_lights = {},
       .spot_lights = {}},
      snapshot);
  REQUIRE(result == granit::scene::multi_view_error::none);
  return snapshot;
}

} // namespace

TEST_CASE("Scene PBR 描述沿用可见排序并转换显式输入") {
  const auto snapshot = make_snapshot();
  granit::scene::scene_pbr_pass_desc desc;
  REQUIRE(granit::scene::build_scene_pbr_pass_desc(snapshot, 0, 3, 4, desc) ==
          granit::scene::scene_pbr_error::none);
  CHECK(desc.pbr.color == 3);
  CHECK(desc.pbr.depth == 4);
  CHECK(desc.pbr.view.camera_position == granit::material::pbr_float3{0, 0, 2});
  CHECK(desc.pbr.light.direction_to_light == granit::material::pbr_float3{0, 0, 1});
  REQUIRE(desc.renderable_indices.size() == 2);
  CHECK(desc.renderable_indices[0] == 1);
  CHECK(desc.renderable_indices[1] == 0);
  REQUIRE(desc.pbr.objects.size() == 2);
  CHECK(desc.pbr.objects[0].object_id == 10);
  CHECK(desc.pbr.objects[1].object_id == 20);
}

TEST_CASE("Scene PBR Pass 声明附件并保留可见索引") {
  const auto snapshot = make_snapshot();
  granit::render_graph::serial_graph graph;
  const auto color = graph.import_texture_view(101, true);
  const auto depth = graph.import_texture_view(102);
  granit::scene::scene_pbr_error error{};
  const auto pass = granit::scene::add_scene_pbr_graph_pass(
      graph, snapshot, 0, color, depth,
      [](granit::render_graph::pass_context&, const granit::material::pbr_frame_constants&,
         std::span<const granit::material::pbr_object_constants>,
         std::span<const std::uint32_t>) { return GRANIT_SUCCESS; },
      error);
  REQUIRE(error == granit::scene::scene_pbr_error::none);
  REQUIRE(pass != granit::render_graph::invalid_pass_id);
  const auto diagnostics = graph.diagnostics();
  REQUIRE(diagnostics.compilation.succeeded());
  CHECK(diagnostics.compilation.execution_order == std::vector{pass});
}

TEST_CASE("Scene PBR 适配拒绝方向光数量和不完整参数") {
  auto snapshot = make_snapshot(0);
  granit::scene::scene_pbr_pass_desc desc;
  CHECK(granit::scene::build_scene_pbr_pass_desc(snapshot, 0, 1,
                                                 granit::render_graph::invalid_resource_id, desc) ==
        granit::scene::scene_pbr_error::directional_light_count);
  snapshot = make_snapshot(2);
  CHECK(granit::scene::build_scene_pbr_pass_desc(snapshot, 0, 1,
                                                 granit::render_graph::invalid_resource_id, desc) ==
        granit::scene::scene_pbr_error::directional_light_count);
  CHECK(granit::scene::build_scene_pbr_pass_desc(snapshot, 2, 1,
                                                 granit::render_graph::invalid_resource_id, desc) ==
        granit::scene::scene_pbr_error::invalid_view);
}
