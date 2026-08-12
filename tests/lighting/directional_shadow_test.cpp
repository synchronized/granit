// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/directional_shadow.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>

namespace {

granit::scene::multi_view_snapshot make_shadow_snapshot(std::uint64_t light_layer = 1) {
  const auto identity = granit::math::identity_matrix4;
  granit::scene::view_input view{.view = identity,
                                 .projection = identity,
                                 .view_projection = identity,
                                 .camera_position = {},
                                 .area = {},
                                 .layer_mask = 1};
  std::array renderables{granit::scene::renderable_input{.model = identity,
                                                         .normal_matrix = identity,
                                                         .bounds = {{0, 0, 0}, 0.5F},
                                                         .layer_mask = 1,
                                                         .sort_key = 2,
                                                         .payload = 11,
                                                         .object_id = 22},
                         granit::scene::renderable_input{.model = identity,
                                                         .normal_matrix = identity,
                                                         .bounds = {{3, 0, 0}, 0.5F},
                                                         .layer_mask = 1,
                                                         .sort_key = 1,
                                                         .payload = 33,
                                                         .object_id = 44}};
  const std::array lights{
      granit::scene::directional_light_input{{0, 0, 1}, {1, 1, 1}, light_layer}};
  granit::scene::multi_view_snapshot snapshot;
  REQUIRE(granit::scene::build_multi_view_snapshot({.views = std::span{&view, 1},
                                                    .renderables = renderables,
                                                    .directional_lights = lights,
                                                    .point_lights = {},
                                                    .spot_lights = {}},
                                                   snapshot) ==
          granit::scene::multi_view_error::none);
  return snapshot;
}

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("方向光阴影从独立光源视锥筛选全部投影者") {
  const auto snapshot = make_shadow_snapshot();
  granit::lighting::directional_shadow_pass_desc desc;
  REQUIRE(granit::lighting::build_directional_shadow_pass_desc(snapshot, 0, 0,
                                                               {.focus = {},
                                                                .half_width = 5,
                                                                .half_height = 5,
                                                                .near_plane = 1,
                                                                .far_plane = 20,
                                                                .light_distance = 10},
                                                               7, desc) ==
          granit::lighting::directional_shadow_error::none);
  REQUIRE(desc.casters.size() == 2);
  CHECK(desc.casters[0].object_id == 44);
  CHECK(desc.casters[0].source_index == 1);
  CHECK(desc.casters[1].payload == 11);

  granit::math::float3 focus_clip;
  REQUIRE(granit::math::transform_point(desc.frame.light_view_projection, {}, focus_clip));
  CHECK(focus_clip.x == Catch::Approx(0.0F));
  CHECK(focus_clip.y == Catch::Approx(0.0F));
  CHECK(focus_clip.z > 0.0F);
  CHECK(focus_clip.z < 1.0F);
}

TEST_CASE("方向光阴影拒绝不可见光源和非法描述") {
  const auto snapshot = make_shadow_snapshot(2);
  granit::lighting::directional_shadow_pass_desc output;
  output.depth = 99;
  CHECK(granit::lighting::build_directional_shadow_pass_desc(snapshot, 0, 0, {}, 7, output) ==
        granit::lighting::directional_shadow_error::light_not_visible);
  CHECK(output.depth == 99);

  const auto visible = make_shadow_snapshot();
  CHECK(granit::lighting::build_directional_shadow_pass_desc(visible, 0, 0, {.half_width = 0}, 7,
                                                             output) ==
        granit::lighting::directional_shadow_error::invalid_volume);
  CHECK(granit::lighting::build_directional_shadow_pass_desc(
            visible, 0, 0, {}, granit::render_graph::invalid_resource_id, output) ==
        granit::lighting::directional_shadow_error::invalid_depth);
}

TEST_CASE("方向光Shadow Pass声明深度写入并复制输入") {
  const auto snapshot = make_shadow_snapshot();
  granit::render_graph::serial_graph graph;
  const auto depth = graph.import_texture_view(123, true, "Shadow Depth");
  granit::lighting::directional_shadow_pass_desc desc;
  REQUIRE(granit::lighting::build_directional_shadow_pass_desc(snapshot, 0, 0, {}, depth, desc) ==
          granit::lighting::directional_shadow_error::none);
  bool called = false;
  const auto pass = granit::lighting::add_directional_shadow_graph_pass(
      graph, desc,
      [&](granit::render_graph::pass_context& context,
          const granit::lighting::shadow_frame_constants&,
          std::span<const granit::lighting::shadow_caster> casters) {
        called = true;
        CHECK(context.texture_view(depth) == 123);
        REQUIRE(casters.size() == 2);
        CHECK(casters[0].object_id == 44);
        return GRANIT_SUCCESS;
      });
  REQUIRE(pass != granit::render_graph::invalid_pass_id);
  desc.casters[0].object_id = 999;

  const auto diagnostics = graph.diagnostics();
  REQUIRE(diagnostics.compilation.succeeded());
  REQUIRE(diagnostics.compilation.resource_lifetimes.size() == 1);
  CHECK(diagnostics.compilation.resource_lifetimes[depth].used);

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-shadow-tests"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  const auto result = graph.execute(renderer.native_handle());
  REQUIRE(result.succeeded());
  CHECK(called);
  REQUIRE(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
          GRANIT_SUCCESS);
}

TEST_CASE("方向光Shadow Pass拒绝不完整描述") {
  granit::render_graph::serial_graph graph;
  CHECK(granit::lighting::add_directional_shadow_graph_pass(graph, {}, {}) ==
        granit::render_graph::invalid_pass_id);
}
