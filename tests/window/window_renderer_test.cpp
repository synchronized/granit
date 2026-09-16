// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/window.hpp>

#include <catch2/catch_all.hpp>

#include "../support/swapchain_frame.h"

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

  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "granit-window-renderer",
                           .presentation = granit::presentation_mode::enabled});
  if (environment_unavailable(renderer_result))
    SKIP("当前环境不支持 Vulkan Win32 Swapchain");
  REQUIRE(renderer_result == granit::result::success);
  granit::surface surface;
  REQUIRE(window.create_surface(renderer.native_handle(), surface) == granit::result::success);
  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                               {.width = 96, .height = 72}) == granit::result::success);
  granit::frame_context frame_context;
  REQUIRE(frame_context.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(granit::tests::render_clear_frames(swapchain, frame_context, 96, 72, 3) ==
          granit::result::success);
}

} // namespace
#endif

#if defined(GRANIT_TEST_HAS_WAYLAND)
namespace {

bool wayland_environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

TEST_CASE("Wayland Window component 可以连接 Renderer Surface 和 Swapchain",
          "[window][renderer][wayland]") {
  granit::window_system window_system;
  const auto system_result = window_system.initialize({.backend = granit::window_backend::wayland});
  if (system_result == granit::result::backend_unavailable)
    SKIP("当前环境没有可用且支持 xdg-shell 的 Wayland compositor");
  REQUIRE(system_result == granit::result::success);
  granit::window window;
  REQUIRE(window.initialize(window_system.native_handle(),
                            {.title = "Granit Wayland Window Renderer Test",
                             .width = 96,
                             .height = 72,
                             .flags = 0}) == granit::result::success);

  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "granit-wayland-window-renderer",
                           .presentation = granit::presentation_mode::enabled});
  if (wayland_environment_unavailable(renderer_result))
    SKIP("当前环境不支持 Vulkan Wayland Swapchain");
  REQUIRE(renderer_result == granit::result::success);
  granit::surface surface;
  REQUIRE(window.create_surface(renderer.native_handle(), surface) == granit::result::success);
  granit::swapchain swapchain;
  const auto swapchain_result = swapchain.initialize(
      renderer.native_handle(), surface.native_handle(), {.width = 96, .height = 72});
  if (wayland_environment_unavailable(swapchain_result))
    SKIP("当前环境不支持 Vulkan Wayland Swapchain");
  REQUIRE(swapchain_result == granit::result::success);
  granit::frame_context frame_context;
  REQUIRE(frame_context.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(granit::tests::render_clear_frames(swapchain, frame_context, 96, 72, 3) ==
          granit::result::success);
}

} // namespace
#endif

#if defined(GRANIT_TEST_HAS_XCB)
namespace {

bool xcb_environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

TEST_CASE("XCB Window component 可以连接 Renderer Surface 和 Swapchain",
          "[window][renderer][xcb]") {
  granit::window_system window_system;
  const auto window_system_result =
      window_system.initialize({.backend = granit::window_backend::xcb});
  if (window_system_result == granit::result::backend_unavailable)
    SKIP("当前环境没有可用的 XCB display");
  REQUIRE(window_system_result == granit::result::success);
  granit::window window;
  REQUIRE(
      window.initialize(
          window_system.native_handle(),
          {.title = "Granit XCB Window Renderer Test", .width = 96, .height = 72, .flags = 0}) ==
      granit::result::success);

  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "granit-xcb-window-renderer",
                           .presentation = granit::presentation_mode::enabled});
  if (xcb_environment_unavailable(renderer_result))
    SKIP("当前环境不支持 Vulkan XCB Swapchain");
  REQUIRE(renderer_result == granit::result::success);
  granit::surface surface;
  REQUIRE(window.create_surface(renderer.native_handle(), surface) == granit::result::success);
  granit::swapchain swapchain;
  const auto swapchain_result = swapchain.initialize(
      renderer.native_handle(), surface.native_handle(), {.width = 96, .height = 72});
  if (xcb_environment_unavailable(swapchain_result))
    SKIP("当前环境不支持 Vulkan XCB Swapchain");
  REQUIRE(swapchain_result == granit::result::success);
  granit::frame_context frame_context;
  REQUIRE(frame_context.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(granit::tests::render_clear_frames(swapchain, frame_context, 96, 72, 3) ==
          granit::result::success);
}

} // namespace
#endif
