// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>

#include <catch2/catch_all.hpp>

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

TEST_CASE("Surface创建把空Renderer归类为无效句柄", "[surface][contract]") {
  const granit_win32_surface_desc desc{GRANIT_WIN32_SURFACE_DESC_VERSION_1_SIZE,
                                       reinterpret_cast<void*>(1), reinterpret_cast<void*>(1)};
  granit_surface handle = UINT64_C(1);
  CHECK(granit_surface_create_win32(GRANIT_NULL_HANDLE, &desc, &handle) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(handle == GRANIT_NULL_HANDLE);

  granit::surface surface;
  CHECK(surface.initialize_win32(GRANIT_NULL_HANDLE, {.instance = reinterpret_cast<void*>(1),
                                                      .window = reinterpret_cast<void*>(1)}) ==
        granit::result::invalid_handle);
}

TEST_CASE("Linux Surface 公共入口验证原生描述", "[surface][xcb][wayland]") {
  granit_surface output = UINT64_C(42);
  granit_xcb_surface_desc xcb = GRANIT_XCB_SURFACE_DESC_INIT;
  CHECK(granit_surface_create_xcb(UINT64_C(1), &xcb, &output) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(output == GRANIT_NULL_HANDLE);
  xcb.connection = &output;
  xcb.window = UINT32_C(1);
  CHECK(granit_surface_create_xcb(UINT64_C(1), &xcb, &output) == GRANIT_ERROR_INVALID_HANDLE);

  granit_wayland_surface_desc wayland = GRANIT_WAYLAND_SURFACE_DESC_INIT;
  CHECK(granit_surface_create_wayland(UINT64_C(1), &wayland, &output) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  wayland.display = &xcb;
  wayland.surface = &wayland;
  CHECK(granit_surface_create_wayland(UINT64_C(1), &wayland, &output) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Canvas Surface 校验 CSS selector 契约", "[surface][canvas][contract]") {
  granit_surface output = UINT64_C(42);
  CHECK(granit_surface_create_canvas(UINT64_C(1), nullptr, &output) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_canvas_surface_desc desc = GRANIT_CANVAS_SURFACE_DESC_INIT;
  desc.selector_length = 1;
  CHECK(granit_surface_create_canvas(UINT64_C(1), &desc, &output) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.selector = "#canvas";
  desc.selector_length = 0;
  CHECK(granit_surface_create_canvas(UINT64_C(1), &desc, &output) == GRANIT_ERROR_INVALID_ARGUMENT);
  constexpr char embedded_zero[] = {'#', 'a', '\0', 'b'};
  desc.selector = embedded_zero;
  desc.selector_length = sizeof(embedded_zero);
  CHECK(granit_surface_create_canvas(UINT64_C(1), &desc, &output) == GRANIT_ERROR_INVALID_ARGUMENT);

  desc = GRANIT_CANVAS_SURFACE_DESC_INIT;
  CHECK(granit_surface_create_canvas(UINT64_C(1), &desc, &output) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(output == GRANIT_NULL_HANDLE);
}

TEST_CASE("Vulkan Renderer 明确拒绝 Canvas Surface", "[surface][canvas]") {
  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "granit-canvas-surface-tests",
                           .surface_types = granit::surface_type::canvas});
  if (renderer_result == granit::result::backend_unavailable ||
      renderer_result == granit::result::incompatible_driver ||
      renderer_result == granit::result::no_suitable_device) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  CHECK(surface.initialize_canvas(renderer.native_handle()) == granit::result::unsupported);
  CHECK_FALSE(surface.valid());
}

#if defined(_WIN32)
bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

class test_window {
public:
  test_window()
      : instance_(GetModuleHandleW(nullptr)),
        window_(CreateWindowExW(0, L"STATIC", L"Granit Surface Test", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 64, 64, nullptr, nullptr, instance_,
                                nullptr)) {}

  ~test_window() {
    if (window_ != nullptr) {
      DestroyWindow(window_);
    }
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

TEST_CASE("Win32 Surface 支持创建、移动和销毁", "[surface][win32]") {
  test_window window;
  REQUIRE(window.valid());

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize(
      {.application_name = "granit-surface-tests", .surface_types = granit::surface_type::win32});
  if (environment_unavailable(renderer_result)) {
    SKIP("当前运行环境不支持 Vulkan Win32 Surface");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize_win32(renderer.native_handle(),
                                   {.instance = window.instance(), .window = window.window()}) ==
          granit::result::success);
  REQUIRE(surface.valid());
  REQUIRE(surface.renderer_handle() == renderer.native_handle());

  granit::surface moved{std::move(surface)};
  CHECK_FALSE(surface.valid());
  CHECK(moved.valid());
  REQUIRE(renderer.reset() == granit::result::success);
  CHECK(moved.reset() == granit::result::invalid_handle);
  CHECK_FALSE(moved.valid());
}

TEST_CASE("Renderer 未启用 Win32 输出时拒绝创建 Surface", "[surface][win32]") {
  test_window window;
  REQUIRE(window.valid());

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-surface-tests"});
  if (environment_unavailable(renderer_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  CHECK(surface.initialize_win32(renderer.native_handle(),
                                 {.instance = window.instance(), .window = window.window()}) ==
        granit::result::unsupported);
  CHECK_FALSE(surface.valid());
}

TEST_CASE("Renderer 销毁时自动使所属 Surface 失效", "[surface][lifetime]") {
  test_window window;
  REQUIRE(window.valid());

  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.surface_types = GRANIT_SURFACE_TYPE_WIN32_BIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto renderer_result = granit_renderer_create(&renderer_desc, &renderer);
  if (environment_unavailable(granit::from_native(renderer_result))) {
    SKIP("当前运行环境不支持 Vulkan Win32 Surface");
  }
  REQUIRE(renderer_result == GRANIT_SUCCESS);

  granit_win32_surface_desc surface_desc = GRANIT_WIN32_SURFACE_DESC_INIT;
  surface_desc.instance = window.instance();
  surface_desc.window = window.window();
  granit_surface surface = GRANIT_NULL_HANDLE;
  REQUIRE(granit_surface_create_win32(renderer, &surface_desc, &surface) == GRANIT_SUCCESS);

  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_surface_destroy(renderer, surface) == GRANIT_ERROR_INVALID_HANDLE);
}
#endif

} // namespace
