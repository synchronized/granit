// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer.hpp>
#include <granit/surface.hpp>
#include <granit/swapchain.hpp>

#include <catch2/catch_all.hpp>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

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
  const auto renderer_result = renderer.initialize(
      {.application_name = "granit-swapchain-tests", .surface_types = granit::surface_type::win32});
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
