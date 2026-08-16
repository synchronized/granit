// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/window.hpp>

#include <catch2/catch_all.hpp>

#if defined(_WIN32)
namespace {

bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

TEST_CASE("Window component 可以连接 Renderer Surface 和 Swapchain", "[window][renderer]") {
  granit::window_system window_system;
  REQUIRE(window_system.initialize() == granit::result::success);
  granit::window window;
  REQUIRE(window.initialize(
              window_system.native_handle(),
              {.title = "Granit Window Renderer Test", .width = 96, .height = 72, .flags = 0}) ==
          granit::result::success);
  void* instance = nullptr;
  void* native_window = nullptr;
  REQUIRE(window.native_win32(instance, native_window) == granit::result::success);

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize(
      {.application_name = "granit-window-renderer", .surface_types = granit::surface_type::win32});
  if (environment_unavailable(renderer_result))
    SKIP("当前环境不支持 Vulkan Win32 Swapchain");
  REQUIRE(renderer_result == granit::result::success);
  granit::surface surface;
  REQUIRE(surface.initialize_win32(renderer.native_handle(), {instance, native_window}) ==
          granit::result::success);
  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                               {.width = 96, .height = 72}) == granit::result::success);
  granit::acquired_frame frame;
  REQUIRE(swapchain.acquire(frame) == granit::result::success);
  REQUIRE(swapchain.cancel(frame) == granit::result::success);
}

} // namespace
#endif
