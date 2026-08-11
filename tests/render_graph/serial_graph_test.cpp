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

TEST_CASE("串行 Render Graph 创建并回收瞬态资源") {
  granit::render_graph::serial_graph graph;
  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  buffer_desc.size = 256;
  const auto buffer = graph.create_transient_buffer(buffer_desc);

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  texture_desc.width = 4;
  texture_desc.height = 4;
  const auto texture = graph.create_transient_texture(texture_desc);
  granit_buffer resolved_buffer = GRANIT_NULL_HANDLE;
  granit_texture_view resolved_view = GRANIT_NULL_HANDLE;

  static_cast<void>(
      graph.add_pass({.side_effect = true,
                      .accesses = {{buffer, granit::render_graph::access_type::write},
                                   {texture, granit::render_graph::access_type::write}}},
                     [&, buffer, texture](granit::render_graph::pass_context& context) {
                       resolved_buffer = context.buffer(buffer);
                       resolved_view = context.texture_view(texture);
                       CHECK(resolved_buffer != GRANIT_NULL_HANDLE);
                       CHECK(resolved_view != GRANIT_NULL_HANDLE);
                       return granit_command_recorder_fill_buffer(
                           context.renderer(), context.recorder(), resolved_buffer, 0, 256, 0);
                     }));

  granit::renderer renderer;
  const auto initialize =
      renderer.initialize({.application_name = "granit-render-graph-transients"});
  if (environment_unavailable(initialize)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialize == granit::result::success);

  const auto result = graph.execute(renderer.native_handle());
  REQUIRE(result.succeeded());
  REQUIRE(result.recorder != GRANIT_NULL_HANDLE);
  CHECK(granit_buffer_destroy(renderer.native_handle(), resolved_buffer) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_texture_view_destroy(renderer.native_handle(), resolved_view) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
        GRANIT_SUCCESS);
}

TEST_CASE("串行 Render Graph 在瞬态资源创建失败时不执行 Pass") {
  granit::render_graph::serial_graph graph;
  granit_buffer_desc invalid_desc = GRANIT_BUFFER_DESC_INIT;
  const auto buffer = graph.create_transient_buffer(invalid_desc);
  bool called = false;
  static_cast<void>(graph.add_pass(
      {.side_effect = true, .accesses = {{buffer, granit::render_graph::access_type::write}}},
      [&](granit::render_graph::pass_context&) {
        called = true;
        return GRANIT_SUCCESS;
      }));

  granit::renderer renderer;
  const auto initialize =
      renderer.initialize({.application_name = "granit-render-graph-create-error"});
  if (environment_unavailable(initialize)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialize == granit::result::success);

  const auto result = graph.execute(renderer.native_handle());
  CHECK(result.result == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(result.phase == granit::render_graph::execution_phase::create_resources);
  CHECK_FALSE(called);
  CHECK(result.recorder == GRANIT_NULL_HANDLE);
}
