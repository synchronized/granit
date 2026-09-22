// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>
#include <granit/window.hpp>
#include <granit/window/native.h>
#include <granit/window/native.hpp>

#include <catch2/catch_all.hpp>

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#elif defined(GRANIT_TEST_HAS_XCB)
#include <xcb/xcb.h>
#endif

TEST_CASE("Window 组件骨架保持确定的失败与输出语义", "[window]") {
  granit_window_system system = UINT64_C(42);
  CHECK(granit_window_system_create(nullptr, &system) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(system == GRANIT_NULL_HANDLE);

  granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
#if defined(_WIN32)
  REQUIRE(granit_window_system_create(&system_desc, &system) == GRANIT_SUCCESS);
  REQUIRE(system != GRANIT_NULL_HANDLE);

  granit_window window = GRANIT_NULL_HANDLE;
  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.title = "Granit Window Test";
  window_desc.title_length = 18;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = GRANIT_WINDOW_HIGH_DPI_BIT;
  REQUIRE(granit_window_create(system, &window_desc, &window) == GRANIT_SUCCESS);
  REQUIRE(window != GRANIT_NULL_HANDLE);

  granit_window_native_win32 native = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  REQUIRE(granit_window_get_native_win32(system, window, &native) == GRANIT_SUCCESS);
  CHECK(native.instance != nullptr);
  CHECK(native.window != nullptr);

  granit_window_state state = GRANIT_WINDOW_STATE_INIT;
  REQUIRE(granit_window_get_state(system, window, &state) == GRANIT_SUCCESS);
  CHECK(state.width > 0);
  CHECK(state.height > 0);
  CHECK(state.framebuffer_width > 0);
  CHECK(state.framebuffer_height > 0);
  CHECK(state.content_scale_horizontal > 0.0F);
  CHECK(state.content_scale_vertical > 0.0F);
  CHECK(granit_window_get_state(system, window, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
  state = GRANIT_WINDOW_STATE_INIT;
  state.struct_size = GRANIT_WINDOW_STATE_VERSION_1_SIZE - 1;
  CHECK(granit_window_get_state(system, window, &state) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS) {
    event = GRANIT_WINDOW_EVENT_INIT;
  }
  REQUIRE(SetWindowPos(static_cast<HWND>(native.window), nullptr, 0, 0, 128, 96,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE);
  bool saw_resize = false;
  for (int attempt = 0; attempt < 8 && !saw_resize; ++attempt) {
    REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
    event = GRANIT_WINDOW_EVENT_INIT;
    if (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
      saw_resize = event.type == GRANIT_WINDOW_EVENT_RESIZED;
  }
  CHECK(saw_resize);

  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
    event = GRANIT_WINDOW_EVENT_INIT;
  RECT suggested{0, 0, 144, 108};
  SendMessageW(static_cast<HWND>(native.window), WM_DPICHANGED, MAKELONG(144, 144),
               reinterpret_cast<LPARAM>(&suggested));
  bool saw_scale = false;
  for (int attempt = 0; attempt < 8 && !saw_scale; ++attempt) {
    event = GRANIT_WINDOW_EVENT_INIT;
    if (granit_window_poll_event(system, &event) == GRANIT_SUCCESS &&
        event.type == GRANIT_WINDOW_EVENT_SCALE_CHANGED) {
      saw_scale = true;
      CHECK(event.data.scale.horizontal == Catch::Approx(1.5F));
      CHECK(event.data.scale.vertical == Catch::Approx(1.5F));
    }
  }
  CHECK(saw_scale);
  state = GRANIT_WINDOW_STATE_INIT;
  REQUIRE(granit_window_get_state(system, window, &state) == GRANIT_SUCCESS);
  CHECK(state.content_scale_horizontal == Catch::Approx(1.5F));
  CHECK(state.content_scale_vertical == Catch::Approx(1.5F));
  CHECK(state.framebuffer_width > 0);
  CHECK(state.framebuffer_height > 0);

  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
    event = GRANIT_WINDOW_EVENT_INIT;
  SendMessageW(static_cast<HWND>(native.window), WM_KILLFOCUS, 0, 0);
  SendMessageW(static_cast<HWND>(native.window), WM_SETFOCUS, 0, 0);
  for (const auto focused : {UINT32_C(0), UINT32_C(1)}) {
    event = GRANIT_WINDOW_EVENT_INIT;
    REQUIRE(granit_window_poll_event(system, &event) == GRANIT_SUCCESS);
    CHECK(event.type == GRANIT_WINDOW_EVENT_FOCUS_CHANGED);
    CHECK(event.window == window);
    CHECK(event.data.focus.focused == focused);
  }

  REQUIRE(PostMessageW(static_cast<HWND>(native.window), WM_CLOSE, 0, 0) != FALSE);
  event = GRANIT_WINDOW_EVENT_INIT;
  CHECK(granit_window_poll_event(system, &event) == GRANIT_ERROR_NOT_READY);
  REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
  event = GRANIT_WINDOW_EVENT_INIT;
  REQUIRE(granit_window_poll_event(system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
  CHECK(event.window == window);
  granit_window_native_xcb xcb = GRANIT_WINDOW_NATIVE_XCB_INIT;
  xcb.connection = reinterpret_cast<void*>(UINTPTR_MAX);
  xcb.window = UINT32_C(42);
  CHECK(granit_window_get_native_xcb(system, window, &xcb) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(xcb.connection == nullptr);
  CHECK(xcb.window == 0);

  state = GRANIT_WINDOW_STATE_INIT;
  CHECK(granit_window_get_state(system, GRANIT_NULL_HANDLE, &state) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(state.width == 0);

  REQUIRE(granit_window_destroy(system, window) == GRANIT_SUCCESS);
  CHECK(granit_window_destroy(system, window) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_window_system_destroy(system) == GRANIT_SUCCESS);
  CHECK(granit_window_system_destroy(system) == GRANIT_ERROR_INVALID_HANDLE);
#elif defined(GRANIT_TEST_HAS_XCB)
  system_desc.backend = GRANIT_WINDOW_BACKEND_XCB;
  const auto create_result = granit_window_system_create(&system_desc, &system);
  if (create_result == GRANIT_ERROR_BACKEND_UNAVAILABLE)
    SKIP("当前环境没有可用的 XCB display");
  REQUIRE(create_result == GRANIT_SUCCESS);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.title = "Granit XCB Window Test";
  window_desc.title_length = 22;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(system, &window_desc, &window) == GRANIT_SUCCESS);

  granit_window_native_xcb native = GRANIT_WINDOW_NATIVE_XCB_INIT;
  REQUIRE(granit_window_get_native_xcb(system, window, &native) == GRANIT_SUCCESS);
  REQUIRE(native.connection != nullptr);
  REQUIRE(native.window != 0);

  const uint32_t size[] = {128, 96};
  xcb_configure_window(static_cast<xcb_connection_t*>(native.connection), native.window,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, size);
  xcb_flush(static_cast<xcb_connection_t*>(native.connection));
  std::free(xcb_get_input_focus_reply(
      static_cast<xcb_connection_t*>(native.connection),
      xcb_get_input_focus(static_cast<xcb_connection_t*>(native.connection)), nullptr));
  bool saw_resize = false;
  for (int attempt = 0; attempt < 32 && !saw_resize; ++attempt) {
    REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    if (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
      saw_resize = event.type == GRANIT_WINDOW_EVENT_RESIZED && event.window == window;
  }
  CHECK(saw_resize);
  granit_window_state state = GRANIT_WINDOW_STATE_INIT;
  REQUIRE(granit_window_get_state(system, window, &state) == GRANIT_SUCCESS);
  CHECK(state.width == 128);
  CHECK(state.height == 96);
  CHECK(state.framebuffer_width == 128);
  CHECK(state.framebuffer_height == 96);
  CHECK(state.content_scale_horizontal == Catch::Approx(1.0F));
  CHECK(state.content_scale_vertical == Catch::Approx(1.0F));
  REQUIRE(granit_window_destroy(system, window) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(system) == GRANIT_SUCCESS);
#else
  CHECK(granit_window_system_create(&system_desc, &system) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(system == GRANIT_NULL_HANDLE);
#endif
}

#if defined(_WIN32)
TEST_CASE("C++ Window 原生快照直接接收 RAII 对象", "[window][native][cpp]") {
  granit::window_system system;
  REQUIRE(system.initialize() == granit::result::success);
  granit::window window;
  REQUIRE(window.initialize(system, {.title = "Native snapshot", .width = 96, .height = 72}) ==
          granit::result::success);

  granit::window_native_win32 native{};
  REQUIRE(granit::get_native(system, window, native) == granit::result::success);
  CHECK(native.instance != nullptr);
  CHECK(native.window != nullptr);
}
#endif

#if defined(GRANIT_TEST_HAS_WAYLAND)
TEST_CASE("Wayland Window 提供配置后的原生 Surface", "[window][wayland]") {
  granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  system_desc.backend = GRANIT_WINDOW_BACKEND_WAYLAND;
  granit_window_system system = GRANIT_NULL_HANDLE;
  const auto system_result = granit_window_system_create(&system_desc, &system);
  if (system_result == GRANIT_ERROR_BACKEND_UNAVAILABLE)
    SKIP("当前环境没有可用且支持 xdg-shell 的 Wayland compositor");
  REQUIRE(system_result == GRANIT_SUCCESS);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.title = "Granit Wayland Window Test";
  window_desc.title_length = 26;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = GRANIT_WINDOW_VISIBLE_BIT;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(system, &window_desc, &window) == GRANIT_SUCCESS);

  granit_window_native_wayland native = GRANIT_WINDOW_NATIVE_WAYLAND_INIT;
  REQUIRE(granit_window_get_native_wayland(system, window, &native) == GRANIT_SUCCESS);
  REQUIRE(native.display != nullptr);
  REQUIRE(native.surface != nullptr);
  granit_window_state state = GRANIT_WINDOW_STATE_INIT;
  REQUIRE(granit_window_get_state(system, window, &state) == GRANIT_SUCCESS);
  CHECK(state.width > 0);
  CHECK(state.height > 0);
  CHECK(state.framebuffer_width == state.width);
  CHECK(state.framebuffer_height == state.height);
  CHECK(state.content_scale_horizontal == Catch::Approx(1.0F));
  CHECK(state.content_scale_vertical == Catch::Approx(1.0F));
  granit_window_native_xcb xcb = GRANIT_WINDOW_NATIVE_XCB_INIT;
  xcb.connection = reinterpret_cast<void*>(UINTPTR_MAX);
  xcb.window = UINT32_C(42);
  REQUIRE(granit_window_get_native_xcb(system, window, &xcb) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(xcb.connection == nullptr);
  CHECK(xcb.window == 0);

  REQUIRE(granit_window_destroy(system, window) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(system) == GRANIT_SUCCESS);
}
#endif
