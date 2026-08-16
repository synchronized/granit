// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>

#include <catch2/catch_all.hpp>
#include <xcb/xcb.h>

#include <cstdlib>
#include <span>

namespace {

class xcb_test_window {
public:
  xcb_test_window() {
    connection_ = xcb_connect(nullptr, &screen_number_);
    if (connection_ == nullptr || xcb_connection_has_error(connection_) != 0)
      return;

    auto iterator = xcb_setup_roots_iterator(xcb_get_setup(connection_));
    for (int index = 0; index < screen_number_ && iterator.rem != 0; ++index)
      xcb_screen_next(&iterator);
    if (iterator.rem == 0)
      return;
    screen_ = iterator.data;

    window_ = xcb_generate_id(connection_);
    const uint32_t values[] = {screen_->black_pixel, XCB_EVENT_MASK_STRUCTURE_NOTIFY};
    xcb_create_window(connection_, XCB_COPY_FROM_PARENT, window_, screen_->root, 0, 0, 96, 72, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen_->root_visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    xcb_map_window(connection_, window_);
    xcb_flush(connection_);

    // 用一次同步请求保证窗口创建错误已由 X server 返回。
    auto cookie = xcb_get_geometry(connection_, window_);
    auto* reply = xcb_get_geometry_reply(connection_, cookie, nullptr);
    valid_ = reply != nullptr;
    std::free(reply);
  }

  ~xcb_test_window() {
    if (connection_ != nullptr) {
      if (window_ != XCB_WINDOW_NONE)
        xcb_destroy_window(connection_, window_);
      xcb_disconnect(connection_);
    }
  }

  xcb_test_window(const xcb_test_window&) = delete;
  xcb_test_window& operator=(const xcb_test_window&) = delete;

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] void* connection() const noexcept { return connection_; }
  [[nodiscard]] uint32_t window() const noexcept { return window_; }

private:
  xcb_connection_t* connection_{};
  xcb_screen_t* screen_{};
  int screen_number_{};
  xcb_window_t window_{XCB_WINDOW_NONE};
  bool valid_{};
};

bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

TEST_CASE("XCB Surface 可以完成 Swapchain 清屏和 Present", "[swapchain][xcb]") {
  xcb_test_window window;
  if (!window.valid())
    SKIP("当前环境没有可用的 XCB display");

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize(
      {.application_name = "granit-xcb-swapchain-tests", .surface_types = granit::surface_type::xcb});
  if (environment_unavailable(renderer_result))
    SKIP("当前环境不支持 Vulkan XCB Swapchain");
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize_xcb(renderer.native_handle(),
                                 {.connection = window.connection(), .window = window.window()}) ==
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

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.03F, .green = 0.09F, .blue = 0.18F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {.width = info.width, .height = info.height}};
  REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit(frame) == granit::result::success);
  REQUIRE(swapchain.present(frame) == granit::result::success);
}

} // namespace
