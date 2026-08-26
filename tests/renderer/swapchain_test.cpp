// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/frame_context.h>
#include <granit/renderer/frame_context.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>

#include <catch2/catch_all.hpp>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

TEST_CASE("Swapchain创建把空父句柄归类为无效句柄", "[swapchain][contract]") {
  const granit_swapchain_desc desc = GRANIT_SWAPCHAIN_DESC_INIT;
  granit_swapchain handle = UINT64_C(1);
  CHECK(granit_swapchain_create(GRANIT_NULL_HANDLE, UINT64_C(1), &desc, &handle) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(handle == GRANIT_NULL_HANDLE);
  CHECK(granit_swapchain_create(UINT64_C(1), GRANIT_NULL_HANDLE, &desc, &handle) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(handle == GRANIT_NULL_HANDLE);

  granit::swapchain swapchain;
  CHECK(swapchain.initialize(GRANIT_NULL_HANDLE, UINT64_C(1), {}) ==
        granit::result::invalid_handle);
  CHECK(swapchain.initialize(UINT64_C(1), GRANIT_NULL_HANDLE, {}) ==
        granit::result::invalid_handle);
}

#if defined(_WIN32)
class swapchain_test_window {
public:
  swapchain_test_window()
      : instance_(GetModuleHandleW(nullptr)),
        window_(CreateWindowExW(0, L"STATIC", L"Granit Swapchain Test", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 96, 72, nullptr, nullptr, instance_,
                                nullptr)) {}
  ~swapchain_test_window() {
    if (window_ != nullptr) {
      DestroyWindow(window_);
    }
  }
  swapchain_test_window(const swapchain_test_window&) = delete;
  swapchain_test_window& operator=(const swapchain_test_window&) = delete;

  [[nodiscard]] bool valid() const noexcept { return window_ != nullptr; }
  [[nodiscard]] void* instance() const noexcept { return instance_; }
  [[nodiscard]] void* window() const noexcept { return window_; }

private:
  HINSTANCE instance_{};
  HWND window_{};
};

bool swapchain_environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

TEST_CASE("Swapchain 支持创建、查询、重建和销毁", "[swapchain][win32]") {
  swapchain_test_window window;
  REQUIRE(window.valid());

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-swapchain-tests",
                                                    .enable_validation = true,
                                                    .surface_types = granit::surface_type::win32});
  if (swapchain_environment_unavailable(renderer_result)) {
    SKIP("当前运行环境不支持 Vulkan Win32 Swapchain");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize_win32(renderer.native_handle(),
                                   {.instance = window.instance(), .window = window.window()}) ==
          granit::result::success);

  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(
              renderer.native_handle(), surface.native_handle(),
              {.width = 96, .height = 72, .presentation = granit::present_mode::mailbox}) ==
          granit::result::success);

  granit::swapchain_info info;
  REQUIRE(swapchain.query_info(info) == granit::result::success);
  CHECK(info.width > 0);
  CHECK(info.height > 0);
  CHECK(info.image_count >= 2);
  CHECK(info.format != granit::texture_format::undefined);

  granit_texture before_minimize_texture = GRANIT_NULL_HANDLE;
  granit_texture_view before_minimize_view = GRANIT_NULL_HANDLE;
  REQUIRE(swapchain.backbuffer(0, before_minimize_texture, before_minimize_view) ==
          granit::result::success);
  CHECK(swapchain.recreate({.width = 0, .height = 0}) == granit::result::not_ready);
  granit_texture after_minimize_texture = GRANIT_NULL_HANDLE;
  granit_texture_view after_minimize_view = GRANIT_NULL_HANDLE;
  REQUIRE(swapchain.backbuffer(0, after_minimize_texture, after_minimize_view) ==
          granit::result::success);
  CHECK(after_minimize_texture == before_minimize_texture);
  CHECK(after_minimize_view == before_minimize_view);

  granit::acquired_frame frame;
  REQUIRE(swapchain.acquire(frame) == granit::result::success);
  REQUIRE(frame.valid());
  granit::frame_info frame_info;
  REQUIRE(frame.query_info(frame_info) == granit::result::success);
  CHECK(frame_info.frame_slot < frame_info.frame_slot_count);
  CHECK(frame_info.frame_slot_count == GRANIT_DEFAULT_FRAMES_IN_FLIGHT);
  granit_frame_info native_frame_info = GRANIT_FRAME_INFO_INIT;
  native_frame_info.reserved[0] = 1;
  REQUIRE(granit_frame_get_info(renderer.native_handle(), swapchain.native_handle(), frame.handle,
                                &native_frame_info) == GRANIT_SUCCESS);
  CHECK(native_frame_info.reserved[0] == 0);
  auto undersized_frame_info = native_frame_info;
  undersized_frame_info.struct_size = GRANIT_FRAME_INFO_VERSION_1_SIZE - 1;
  CHECK(granit_frame_get_info(renderer.native_handle(), swapchain.native_handle(), frame.handle,
                              &undersized_frame_info) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_frame_get_info(renderer.native_handle(), renderer.native_handle(), frame.handle,
                              &native_frame_info) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(swapchain.recreate({.width = 96, .height = 72}) == granit::result::invalid_argument);
  granit_frame_context_desc context_desc = GRANIT_FRAME_CONTEXT_DESC_INIT;
  granit_frame_context context = GRANIT_NULL_HANDLE;
  REQUIRE(granit_frame_context_create(renderer.native_handle(), &context_desc, &context) ==
          GRANIT_SUCCESS);
  granit_command_recorder borrowed_recorder = GRANIT_NULL_HANDLE;
  std::uint32_t context_slot{};
  REQUIRE(granit_frame_context_begin(renderer.native_handle(), context, frame.handle,
                                     &borrowed_recorder, &context_slot) == GRANIT_SUCCESS);
  CHECK(context_slot == frame_info.frame_slot);
  CHECK(borrowed_recorder != GRANIT_NULL_HANDLE);
  CHECK(granit_command_recorder_destroy(renderer.native_handle(), borrowed_recorder) ==
        GRANIT_ERROR_UNSUPPORTED);
  granit_command_recorder repeated_recorder = GRANIT_NULL_HANDLE;
  std::uint32_t repeated_slot{};
  CHECK(granit_frame_context_begin(renderer.native_handle(), context, frame.handle,
                                   &repeated_recorder,
                                   &repeated_slot) == GRANIT_ERROR_INVALID_ARGUMENT);
  granit_texture frame_texture{};
  granit_texture_view frame_view{};
  REQUIRE(swapchain.backbuffer(frame.image_index, frame_texture, frame_view) ==
          granit::result::success);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = frame_view;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area.width = info.width;
  rendering.area.height = info.height;
  REQUIRE(granit_command_recorder_begin_rendering(renderer.native_handle(), borrowed_recorder,
                                                  &rendering) == GRANIT_SUCCESS);
  REQUIRE(granit_command_recorder_end_rendering(renderer.native_handle(), borrowed_recorder) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_frame_context_submit(renderer.native_handle(), context, frame.handle) ==
          GRANIT_SUCCESS);
  CHECK(granit_frame_context_submit(renderer.native_handle(), context, frame.handle) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_frame_context_abort(renderer.native_handle(), context, frame.handle) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  const auto presented_frame = frame.handle;
  REQUIRE(swapchain.present(frame) == granit::result::success);
  CHECK_FALSE(frame.valid());
  CHECK(granit_frame_get_info(renderer.native_handle(), swapchain.native_handle(), presented_frame,
                              &native_frame_info) == GRANIT_ERROR_INVALID_HANDLE);

  granit::acquired_frame cancelled;
  REQUIRE(swapchain.acquire(cancelled) == granit::result::success);
  const auto cancelled_frame = cancelled.handle;
  REQUIRE(granit_frame_context_begin(renderer.native_handle(), context, cancelled.handle,
                                     &borrowed_recorder, &context_slot) == GRANIT_SUCCESS);
  REQUIRE(granit_frame_context_abort(renderer.native_handle(), context, cancelled.handle) ==
          GRANIT_SUCCESS);
  REQUIRE(swapchain.cancel(cancelled) == granit::result::success);
  CHECK_FALSE(cancelled.valid());
  CHECK(granit_frame_get_info(renderer.native_handle(), swapchain.native_handle(), cancelled_frame,
                              &native_frame_info) == GRANIT_ERROR_INVALID_HANDLE);
  {
    granit::acquired_frame automatic;
    REQUIRE(swapchain.acquire(automatic) == granit::result::success);
    REQUIRE(granit_frame_context_begin(renderer.native_handle(), context, automatic.handle,
                                       &borrowed_recorder, &context_slot) == GRANIT_SUCCESS);
    REQUIRE(granit_frame_context_abort(renderer.native_handle(), context, automatic.handle) ==
            GRANIT_SUCCESS);
  }
  REQUIRE(swapchain.recreate({.width = 96, .height = 72}) == granit::result::success);
  REQUIRE(granit_frame_context_destroy(renderer.native_handle(), context) == GRANIT_SUCCESS);
  CHECK(granit_frame_context_destroy(renderer.native_handle(), context) ==
        GRANIT_ERROR_INVALID_HANDLE);

  granit::frame_context cpp_context;
  REQUIRE(cpp_context.initialize(renderer.native_handle()) == granit::result::success);
  granit::acquired_frame cpp_frame;
  REQUIRE(swapchain.acquire(cpp_frame) == granit::result::success);
  granit::frame_recording cpp_recording;
  REQUIRE(cpp_context.begin(cpp_frame, cpp_recording) == granit::result::success);
  CHECK(cpp_recording.valid());
  granit::frame_info cpp_frame_info;
  REQUIRE(cpp_frame.query_info(cpp_frame_info) == granit::result::success);
  CHECK(cpp_recording.frame_slot() == cpp_frame_info.frame_slot);
  REQUIRE(swapchain.backbuffer(cpp_frame.image_index, frame_texture, frame_view) ==
          granit::result::success);
  const granit::color_attachment_desc cpp_color{.view = frame_view};
  const granit::rendering_desc cpp_rendering{
      .color_attachments = std::span{&cpp_color, 1},
      .area = {.width = info.width, .height = info.height}};
  REQUIRE(cpp_recording.recorder().begin_rendering(cpp_rendering) == granit::result::success);
  REQUIRE(cpp_recording.recorder().end_rendering() == granit::result::success);
  REQUIRE(cpp_recording.submit() == granit::result::success);
  CHECK_FALSE(cpp_recording.valid());
  REQUIRE(swapchain.present(cpp_frame) == granit::result::success);

  granit::acquired_frame cpp_aborted_frame;
  REQUIRE(swapchain.acquire(cpp_aborted_frame) == granit::result::success);
  {
    granit::frame_recording automatic_abort;
    REQUIRE(cpp_context.begin(cpp_aborted_frame, automatic_abort) == granit::result::success);
  }
  REQUIRE(swapchain.cancel(cpp_aborted_frame) == granit::result::success);
  REQUIRE(cpp_context.reset() == granit::result::success);

  granit_texture old_texture = GRANIT_NULL_HANDLE;
  granit_texture_view old_view = GRANIT_NULL_HANDLE;
  REQUIRE(swapchain.backbuffer(0, old_texture, old_view) == granit::result::success);
  REQUIRE(old_texture != GRANIT_NULL_HANDLE);
  REQUIRE(old_view != GRANIT_NULL_HANDLE);
  CHECK(granit_texture_destroy(renderer.native_handle(), old_texture) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(granit_texture_view_destroy(renderer.native_handle(), old_view) ==
        GRANIT_ERROR_UNSUPPORTED);
  granit_texture invalid_texture = GRANIT_NULL_HANDLE;
  granit_texture_view invalid_view = GRANIT_NULL_HANDLE;
  CHECK(swapchain.backbuffer(info.image_count, invalid_texture, invalid_view) ==
        granit::result::invalid_argument);

  REQUIRE(swapchain.recreate({.width = 128, .height = 96}) == granit::result::success);
  REQUIRE(swapchain.query_info(info) == granit::result::success);
  CHECK(info.width > 0);
  CHECK(info.height > 0);
  CHECK(granit_texture_destroy(renderer.native_handle(), old_texture) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_texture_view_destroy(renderer.native_handle(), old_view) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(swapchain.reset() == granit::result::success);
}

TEST_CASE("Surface 销毁时自动使所属 Swapchain 失效", "[swapchain][lifetime]") {
  swapchain_test_window window;
  REQUIRE(window.valid());

  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "granit-swapchain-lifetime-tests",
                           .surface_types = granit::surface_type::win32});
  if (swapchain_environment_unavailable(renderer_result)) {
    SKIP("当前运行环境不支持 Vulkan Win32 Swapchain");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize_win32(renderer.native_handle(),
                                   {.instance = window.instance(), .window = window.window()}) ==
          granit::result::success);
  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                               {.width = 96, .height = 72}) == granit::result::success);

  REQUIRE(surface.reset() == granit::result::success);
  granit::swapchain_info info;
  CHECK(swapchain.query_info(info) == granit::result::invalid_handle);
}
#endif

} // namespace
