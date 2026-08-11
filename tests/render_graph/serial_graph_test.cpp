// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/serial_graph.h"

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>

#include <catch2/catch_all.hpp>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

#if defined(_WIN32)
class graph_test_window {
public:
  graph_test_window()
      : instance_(GetModuleHandleW(nullptr)),
        window_(CreateWindowExW(0, L"STATIC", L"Granit Graph Test", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 96, 72, nullptr, nullptr, instance_,
                                nullptr)) {}
  ~graph_test_window() {
    if (window_ != nullptr) {
      DestroyWindow(window_);
    }
  }
  graph_test_window(const graph_test_window&) = delete;
  graph_test_window& operator=(const graph_test_window&) = delete;
  [[nodiscard]] bool valid() const noexcept { return window_ != nullptr; }
  [[nodiscard]] void* instance() const noexcept { return instance_; }
  [[nodiscard]] void* window() const noexcept { return window_; }

private:
  HINSTANCE instance_{};
  HWND window_{};
};
#endif

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
  const auto failed = graph.add_pass(
      {.side_effect = true, .accesses = {}},
      [&](granit::render_graph::pass_context&) {
        ++calls;
        return GRANIT_ERROR_UNSUPPORTED;
      },
      "失败 Pass");
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
  CHECK(result.error_pass_name == "失败 Pass");
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

TEST_CASE("Render Graph 诊断保留名称、依赖和生命周期") {
  granit::render_graph::serial_graph graph;
  const auto input = graph.import_buffer(101, false, "输入 Buffer");
  const auto output = graph.import_buffer(202, true, "输出 Buffer");
  const auto producer = graph.add_pass(
      {.accesses = {{input, granit::render_graph::access_type::read},
                    {output, granit::render_graph::access_type::write}}},
      [](granit::render_graph::pass_context&) { return GRANIT_SUCCESS; }, "复制 Pass");
  const auto consumer = graph.add_pass(
      {.side_effect = true, .accesses = {{output, granit::render_graph::access_type::read}}},
      [](granit::render_graph::pass_context&) { return GRANIT_SUCCESS; }, "读取 Pass");

  const auto diagnostics = graph.diagnostics();
  REQUIRE(diagnostics.compilation.succeeded());
  CHECK(diagnostics.pass_names[producer] == "复制 Pass");
  CHECK(diagnostics.pass_names[consumer] == "读取 Pass");
  CHECK(diagnostics.resource_names[input] == "输入 Buffer");
  CHECK(diagnostics.resource_names[output] == "输出 Buffer");
  CHECK(diagnostics.compilation.resource_lifetimes[input].used);
  REQUIRE(diagnostics.compilation.dependencies.size() == 1);
  CHECK(diagnostics.compilation.dependencies[0].before == producer);
  CHECK(diagnostics.compilation.dependencies[0].after == consumer);
}

#if defined(_WIN32)
TEST_CASE("串行 Render Graph 提交 Swapchain Frame", "[render_graph][swapchain]") {
  graph_test_window window;
  REQUIRE(window.valid());
  granit::renderer renderer;
  const auto initialize = renderer.initialize(
      {.application_name = "granit-graph-window", .surface_types = granit::surface_type::win32});
  if (environment_unavailable(initialize) || initialize == granit::result::unsupported) {
    SKIP("当前运行环境不支持 Vulkan Win32 Swapchain");
  }
  REQUIRE(initialize == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize_win32(renderer.native_handle(),
                                   {.instance = window.instance(), .window = window.window()}) ==
          granit::result::success);
  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                               {.width = 96, .height = 72}) == granit::result::success);
  granit::swapchain_info info;
  REQUIRE(swapchain.query_info(info) == granit::result::success);
  granit::acquired_frame frame;
  REQUIRE(swapchain.acquire(frame) == granit::result::success);
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(swapchain.backbuffer(frame.image_index, texture, view) == granit::result::success);

  granit::render_graph::serial_graph graph;
  const auto backbuffer = graph.import_texture_view(view, true, "Backbuffer");
  granit_result record_result = GRANIT_SUCCESS;
  static_cast<void>(graph.add_pass(
      {.side_effect = true, .accesses = {{backbuffer, granit::render_graph::access_type::write}}},
      [&, backbuffer](granit::render_graph::pass_context& context) {
        granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
        color.view = context.texture_view(backbuffer);
        granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
        rendering.color_attachments = &color;
        rendering.color_attachment_count = 1;
        rendering.area.width = info.width;
        rendering.area.height = info.height;
        record_result = granit_command_recorder_begin_rendering(context.renderer(),
                                                                context.recorder(), &rendering);
        if (record_result == GRANIT_SUCCESS) {
          record_result =
              granit_command_recorder_end_rendering(context.renderer(), context.recorder());
        }
        return record_result;
      },
      "窗口清屏"));

  const auto result = graph.execute_frame(renderer.native_handle(), frame.handle);
  INFO("result=" << result.result << ", phase=" << static_cast<int>(result.phase)
                 << ", record=" << record_result);
  REQUIRE(result.succeeded());
  REQUIRE(result.recorder != GRANIT_NULL_HANDLE);
  REQUIRE(swapchain.present(frame) == granit::result::success);
  CHECK(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
        GRANIT_SUCCESS);
}
#endif
