// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/serial_graph.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("串行 Render Graph 限制 Pass 解析未声明资源") {
  granit::render_graph::serial_graph graph;
  const auto buffer = graph.import_buffer(101);
  const auto view = graph.import_texture_view(202);
  REQUIRE(buffer != granit::render_graph::invalid_resource_id);
  REQUIRE(view != granit::render_graph::invalid_resource_id);

  bool called = false;
  static_cast<void>(graph.add_pass(
      {.side_effect = true, .accesses = {{buffer, granit::render_graph::access_type::read}}},
      [&](granit::render_graph::pass_context& context) {
        called = true;
        CHECK(context.buffer(buffer) == 101);
        CHECK(context.texture_view(buffer) == GRANIT_NULL_HANDLE);
        CHECK(context.texture_view(view) == GRANIT_NULL_HANDLE);
        return GRANIT_SUCCESS;
      }));

  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-render-graph-tests"});
  if (environment_unavailable(initialize)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialize == granit::result::success);

  const auto result = graph.execute(renderer.native_handle());
  REQUIRE(result.succeeded());
  CHECK(called);
  REQUIRE(result.recorder != GRANIT_NULL_HANDLE);
  CHECK(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
        GRANIT_SUCCESS);
}

TEST_CASE("串行 Render Graph 在 Pass 失败后停止且不返回 Recorder") {
  granit::render_graph::serial_graph graph;
  int calls = 0;
  const auto failed = graph.add_pass({.side_effect = true, .accesses = {}},
                                     [&](granit::render_graph::pass_context&) {
                                       ++calls;
                                       return GRANIT_ERROR_UNSUPPORTED;
                                     });
  static_cast<void>(graph.add_pass({.side_effect = true, .accesses = {}},
                                   [&](granit::render_graph::pass_context&) {
                                     ++calls;
                                     return GRANIT_SUCCESS;
                                   }));

  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-render-graph-errors"});
  if (environment_unavailable(initialize)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialize == granit::result::success);

  const auto result = graph.execute(renderer.native_handle());
  CHECK(result.result == GRANIT_ERROR_UNSUPPORTED);
  CHECK(result.phase == granit::render_graph::execution_phase::record_pass);
  CHECK(result.error_pass == failed);
  CHECK(result.recorder == GRANIT_NULL_HANDLE);
  CHECK(calls == 1);
}
