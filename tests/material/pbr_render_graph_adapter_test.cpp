// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_render_graph_adapter.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <memory>

namespace {

constexpr granit::material::pbr_matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("PBR Render Graph Pass 声明附件并传递显式常量") {
  granit::render_graph::serial_graph graph;
  const auto color = graph.import_texture_view(101, true, "PBR Color");
  const auto depth = graph.import_texture_view(102, false, "PBR Depth");
  bool called = false;
  granit::material::pbr_graph_pass_desc desc{
      .color = color,
      .depth = depth,
      .view = {.view_projection = identity, .camera_position = {0, 0, 2}},
      .light = {.direction_to_light = {0, 0, 2}, .radiance = {3, 2, 1}},
      .objects = {{.model = identity, .normal_matrix = identity, .object_id = 7}}};
  const auto pass = granit::material::add_pbr_graph_pass(
      graph, desc,
      [&](granit::render_graph::pass_context& context,
          const granit::material::pbr_frame_constants& frame,
          std::span<const granit::material::pbr_object_constants> objects) {
        called = true;
        CHECK(context.texture_view(color) == 101);
        CHECK(context.texture_view(depth) == 102);
        CHECK(frame.direction_to_light[2] == 1.0F);
        REQUIRE(objects.size() == 1);
        CHECK(objects.front().object_id[0] == 7);
        return GRANIT_SUCCESS;
      });
  REQUIRE(pass != granit::render_graph::invalid_pass_id);
  desc.light.direction_to_light = {1, 0, 0};
  desc.objects.front().object_id = 99;

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-pbr-graph-tests"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  const auto result = graph.execute(renderer.native_handle());
  REQUIRE(result.succeeded());
  CHECK(called);
  REQUIRE(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
          GRANIT_SUCCESS);
}

TEST_CASE("PBR Render Graph Pass 随 Graph 释放回调捕获") {
  auto lifetime = std::make_shared<int>(42);
  const std::weak_ptr<int> observer = lifetime;
  {
    granit::render_graph::serial_graph graph;
    const auto color = graph.import_texture_view(101, true);
    const auto pass = granit::material::add_pbr_graph_pass(
        graph,
        {.color = color,
         .view = {.view_projection = identity},
         .light = {},
         .objects = {{.model = identity, .normal_matrix = identity, .object_id = 0}}},
        [lifetime](
            granit::render_graph::pass_context&, const granit::material::pbr_frame_constants&,
            std::span<const granit::material::pbr_object_constants>) { return GRANIT_SUCCESS; });
    REQUIRE(pass != granit::render_graph::invalid_pass_id);
    lifetime.reset();
    CHECK_FALSE(observer.expired());
  }
  CHECK(observer.expired());
}

TEST_CASE("PBR Render Graph Pass 拒绝不完整描述") {
  granit::render_graph::serial_graph graph;
  CHECK(granit::material::add_pbr_graph_pass(graph, {}, {}) ==
        granit::render_graph::invalid_pass_id);
}
