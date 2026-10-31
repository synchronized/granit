// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph.h>
#include <granit/renderer/native_surface.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

struct record_state {
  granit_render_graph_resource resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  granit_result result = GRANIT_SUCCESS;
};

struct window_record_state {
  granit_render_graph_resource resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  granit_result begin_result = GRANIT_SUCCESS;
  granit_result result = GRANIT_SUCCESS;
};

#if defined(_WIN32)
class test_window {
public:
  test_window()
      : instance_(GetModuleHandleW(nullptr)),
        window_(CreateWindowExW(0, L"STATIC", L"Granit Public Graph Test", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 96, 72, nullptr, nullptr, instance_,
                                nullptr)) {}
  ~test_window() {
    if (window_ != nullptr)
      DestroyWindow(window_);
  }
  test_window(const test_window&) = delete;
  test_window& operator=(const test_window&) = delete;
  [[nodiscard]] bool valid() const noexcept { return window_ != nullptr; }
  [[nodiscard]] void* instance() const noexcept { return instance_; }
  [[nodiscard]] void* window() const noexcept { return window_; }

private:
  HINSTANCE instance_{};
  HWND window_{};
};
#endif

granit_result fill_buffer(const granit_render_graph_pass_context* context, void* user_data) {
  auto& state = *static_cast<record_state*>(user_data);
  state.buffer = granit_render_graph_context_buffer(context, state.resource);
  if (state.buffer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  state.result = granit_command_recorder_fill_buffer(
      granit_render_graph_context_renderer(context), granit_render_graph_context_recorder(context),
      state.buffer, 0, 256, 0x12345678U);
  return state.result;
}

granit_result record_window(const granit_render_graph_pass_context* context, void* user_data) {
  auto& state = *static_cast<window_record_state*>(user_data);
  state.view = granit_render_graph_context_texture_view(context, state.resource);
  state.renderer = granit_render_graph_context_renderer(context);
  state.recorder = granit_render_graph_context_recorder(context);
  if (state.view == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = state.view;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachments = &color;
  rendering.color_attachment_count = 1;
  rendering.area.width = state.width;
  rendering.area.height = state.height;
  state.begin_result = granit_command_recorder_begin_rendering(state.renderer, state.recorder, &rendering);
  state.result = state.begin_result;
  if (state.result == GRANIT_SUCCESS)
    state.result = granit_command_recorder_end_rendering(state.renderer, state.recorder);
  return state.result;
}

} // namespace

TEST_CASE("公共 Render Graph 执行回调并在提交后回收瞬态资源") {
  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-public-render-graph"});
  if (environment_unavailable(initialize))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialize == granit::result::success);

  granit_render_graph graph = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_graph_create(nullptr, &graph) == GRANIT_SUCCESS);

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  buffer_desc.size = 256;
  granit_render_graph_resource resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  REQUIRE(granit_render_graph_create_transient_buffer(graph, &buffer_desc, "Transient Buffer", 15,
                                                      &resource) == GRANIT_SUCCESS);

  const granit_render_graph_access_desc access{
      GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE,
      resource,
      GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
      GRANIT_RENDER_GRAPH_USAGE_TRANSFER_DESTINATION_BIT,
      0};
  const granit_render_graph_pass_desc pass_desc{
      GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE,
      GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT,
      "Fill Buffer",
      11,
      &access,
      1,
      0};
  record_state state{.resource = resource};
  granit_render_graph_pass pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  REQUIRE(granit_render_graph_add_pass(graph, &pass_desc, fill_buffer, &state, &pass) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_render_graph_compile(graph) == GRANIT_SUCCESS);

  granit_render_graph_execution_result execution = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
  REQUIRE(granit_render_graph_execute(graph, renderer.native_handle(), GRANIT_NULL_HANDLE,
                                      UINT32_MAX, &execution) == GRANIT_SUCCESS);
  CHECK(state.result == GRANIT_SUCCESS);
  REQUIRE(state.buffer != GRANIT_NULL_HANDLE);
  REQUIRE(execution.recorder != GRANIT_NULL_HANDLE);
  CHECK(granit_buffer_destroy(renderer.native_handle(), state.buffer) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_command_recorder_destroy(renderer.native_handle(), execution.recorder) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_render_graph_destroy(graph) == GRANIT_SUCCESS);
}

TEST_CASE("公共 Render Graph 执行结果定位未初始化读取") {
  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-public-render-graph-errors"});
  if (environment_unavailable(initialize))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialize == granit::result::success);

  granit_render_graph graph = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_graph_create(nullptr, &graph) == GRANIT_SUCCESS);
  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  buffer_desc.size = 256;
  granit_render_graph_resource resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  REQUIRE(granit_render_graph_create_transient_buffer(graph, &buffer_desc, "Uninitialized", 13,
                                                      &resource) == GRANIT_SUCCESS);
  const granit_render_graph_access_desc access{
      GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE,
      resource,
      GRANIT_RENDER_GRAPH_ACCESS_READ_BIT,
      GRANIT_RENDER_GRAPH_USAGE_TRANSFER_SOURCE_BIT,
      0};
  const granit_render_graph_pass_desc pass_desc{
      GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE,
      GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT,
      "Read Before Write",
      17,
      &access,
      1,
      0};
  granit_render_graph_pass pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  REQUIRE(granit_render_graph_add_pass(graph, &pass_desc, [](const auto*, void*) {
            return GRANIT_SUCCESS;
          },
          nullptr, &pass) == GRANIT_SUCCESS);

  granit_render_graph_execution_result execution = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
  CHECK(granit_render_graph_execute(graph, renderer.native_handle(), GRANIT_NULL_HANDLE, UINT32_MAX,
                                    &execution) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(execution.phase == GRANIT_RENDER_GRAPH_EXECUTION_PHASE_COMPILE);
  CHECK(execution.error_pass_name_length == 17U);
  CHECK(std::string_view{execution.error_pass_name, execution.error_pass_name_length} ==
        "Read Before Write");
  CHECK(execution.error_resource_name_length == 13U);
  CHECK(std::string_view{execution.error_resource_name, execution.error_resource_name_length} ==
        "Uninitialized");
  CHECK(granit_render_graph_destroy(graph) == GRANIT_SUCCESS);
}

TEST_CASE("公共 Render Graph 可更新尺寸并重复执行瞬态纹理") {
  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-public-render-graph-resize"});
  if (environment_unavailable(initialize))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialize == granit::result::success);

  granit_render_graph graph = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_graph_create(nullptr, &graph) == GRANIT_SUCCESS);
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 4;
  texture_desc.height = 4;
  granit_render_graph_resource resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  REQUIRE(granit_render_graph_create_transient_texture(graph, &texture_desc, "Resize Target", 12,
                                                       &resource) == GRANIT_SUCCESS);
  const granit_render_graph_access_desc access{
      GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE,
      resource,
      GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
      GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT,
      0};
  const granit_render_graph_pass_desc pass_desc{
      GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE,
      GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT,
      "Resize Pass",
      11,
      &access,
      1,
      0};
  window_record_state state{.resource = resource, .width = 4, .height = 4};
  granit_render_graph_pass pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  REQUIRE(granit_render_graph_add_pass(graph, &pass_desc, record_window, &state, &pass) ==
          GRANIT_SUCCESS);

  auto execute = [&]() {
    granit_render_graph_execution_result result = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
    const auto value = granit_render_graph_execute(graph, renderer.native_handle(),
                                                   GRANIT_NULL_HANDLE, UINT32_MAX, &result);
    if (value == GRANIT_SUCCESS)
      static_cast<void>(granit_command_recorder_destroy(renderer.native_handle(), result.recorder));
    return value;
  };
  REQUIRE(execute() == GRANIT_SUCCESS);
  CHECK(state.result == GRANIT_SUCCESS);

  texture_desc.width = 8;
  texture_desc.height = 8;
  state.width = 8;
  state.height = 8;
  const granit_render_graph_transient_texture_update update{
      GRANIT_RENDER_GRAPH_TRANSIENT_TEXTURE_UPDATE_VERSION_1_SIZE, resource, texture_desc, 0};
  REQUIRE(granit_render_graph_update_transient_texture(graph, &update) == GRANIT_SUCCESS);
  REQUIRE(granit_render_graph_compile(graph) == GRANIT_SUCCESS);
  REQUIRE(execute() == GRANIT_SUCCESS);
  CHECK(state.result == GRANIT_SUCCESS);
  REQUIRE(granit_render_graph_destroy(graph) == GRANIT_SUCCESS);
}

#if defined(_WIN32)
TEST_CASE("公共 Render Graph 执行到窗口 Frame", "[render_graph][swapchain]") {
  test_window window;
  REQUIRE(window.valid());
  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-public-render-graph-window",
                                               .presentation = granit::presentation_mode::enabled});
  if (environment_unavailable(initialize) || initialize == granit::result::unsupported)
    SKIP("当前运行环境不支持 Vulkan Win32 Swapchain");
  REQUIRE(initialize == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize(renderer,
                             granit::surface_desc::win32(window.instance(), window.window())) ==
          granit::result::success);
  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(renderer, surface, {.width = 96, .height = 72}) ==
          granit::result::success);
  granit::swapchain_info info;
  REQUIRE(swapchain.query_info(info) == granit::result::success);
  granit::acquired_frame frame;
  REQUIRE(swapchain.acquire(frame) == granit::result::success);
  granit::swapchain_backbuffer backbuffer;
  REQUIRE(swapchain.backbuffer(frame, backbuffer) == granit::result::success);

  granit_render_graph graph = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_graph_create(nullptr, &graph) == GRANIT_SUCCESS);
  granit_render_graph_resource resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  REQUIRE(granit_render_graph_import_texture_view(graph, backbuffer.view.native_handle(), 1,
                                                  "Backbuffer", 9, &resource) == GRANIT_SUCCESS);
  const granit_render_graph_access_desc access{
      GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE,
      resource,
      GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
      GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT,
      0};
  const granit_render_graph_pass_desc pass_desc{
      GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE,
      GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT,
      "Window Pass",
      11,
      &access,
      1,
      0};
  window_record_state state{.resource = resource, .width = info.width, .height = info.height};
  granit_render_graph_pass pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  REQUIRE(granit_render_graph_add_pass(graph, &pass_desc, record_window, &state, &pass) ==
          GRANIT_SUCCESS);
  granit_render_graph_execution_result execution = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
  const auto execution_result = granit_render_graph_execute(
      graph, renderer.native_handle(), frame.native_handle(), UINT32_MAX, &execution);
  INFO("result=" << execution_result << ", phase=" << execution.phase << ", graph_error="
                  << execution.graph_error << ", callback=" << state.result << ", begin="
                  << state.begin_result << ", renderer=" << state.renderer << ", recorder="
                  << state.recorder << ", view=" << state.view);
  REQUIRE(execution_result == GRANIT_SUCCESS);
  CHECK(state.result == GRANIT_SUCCESS);
  REQUIRE(swapchain.present(frame) == granit::result::success);
  REQUIRE(granit_command_recorder_destroy(renderer.native_handle(), execution.recorder) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_render_graph_destroy(graph) == GRANIT_SUCCESS);
}
#endif
