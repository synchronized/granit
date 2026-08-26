// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>
#include <granit/window.hpp>

#include <catch2/catch_all.hpp>

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#elif defined(GRANIT_TEST_HAS_XCB)
#include <xcb/xcb.h>
#endif

TEST_CASE("Window创建把空Window System归类为无效句柄", "[window][contract]") {
  granit_window_desc desc = GRANIT_WINDOW_DESC_INIT;
  desc.width = 1;
  desc.height = 1;
  granit_window handle = UINT64_C(1);
  CHECK(granit_window_create(GRANIT_NULL_HANDLE, &desc, &handle) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(handle == GRANIT_NULL_HANDLE);

  granit::window window;
  CHECK(window.initialize(GRANIT_NULL_HANDLE, {.title = "", .width = 1, .height = 1}) ==
        granit::result::invalid_handle);
}

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

  void* first = nullptr;
  void* second = nullptr;
  REQUIRE(granit_window_get_win32(system, window, &first, &second) == GRANIT_SUCCESS);
  CHECK(first != nullptr);
  CHECK(second != nullptr);

  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS) {
    event = GRANIT_WINDOW_EVENT_INIT;
  }
  REQUIRE(SetWindowPos(static_cast<HWND>(second), nullptr, 0, 0, 128, 96,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE);
  bool saw_resize = false;
  for (int attempt = 0; attempt < 8 && !saw_resize; ++attempt) {
    event = GRANIT_WINDOW_EVENT_INIT;
    if (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
      saw_resize = event.type == GRANIT_WINDOW_EVENT_RESIZED;
  }
  CHECK(saw_resize);

  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
    event = GRANIT_WINDOW_EVENT_INIT;
  RECT suggested{0, 0, 144, 108};
  SendMessageW(static_cast<HWND>(second), WM_DPICHANGED, MAKELONG(144, 144),
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

  SendMessageW(static_cast<HWND>(second), WM_CLOSE, 0, 0);
  event = GRANIT_WINDOW_EVENT_INIT;
  REQUIRE(granit_window_poll_event(system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
  CHECK(event.window == window);
  uint32_t xcb_window = UINT32_C(42);
  CHECK(granit_window_get_xcb(system, window, &first, &xcb_window) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(first == nullptr);
  CHECK(xcb_window == 0);

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

  void* connection = nullptr;
  uint32_t native_window = 0;
  REQUIRE(granit_window_get_xcb(system, window, &connection, &native_window) == GRANIT_SUCCESS);
  REQUIRE(connection != nullptr);
  REQUIRE(native_window != 0);

  const uint32_t size[] = {128, 96};
  xcb_configure_window(static_cast<xcb_connection_t*>(connection), native_window,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, size);
  xcb_flush(static_cast<xcb_connection_t*>(connection));
  std::free(xcb_get_input_focus_reply(
      static_cast<xcb_connection_t*>(connection),
      xcb_get_input_focus(static_cast<xcb_connection_t*>(connection)), nullptr));
  bool saw_resize = false;
  for (int attempt = 0; attempt < 32 && !saw_resize; ++attempt) {
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    if (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
      saw_resize = event.type == GRANIT_WINDOW_EVENT_RESIZED && event.window == window;
  }
  CHECK(saw_resize);
  REQUIRE(granit_window_destroy(system, window) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(system) == GRANIT_SUCCESS);
#else
  CHECK(granit_window_system_create(&system_desc, &system) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(system == GRANIT_NULL_HANDLE);
#endif
}

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

  void* display = nullptr;
  void* surface = nullptr;
  REQUIRE(granit_window_get_wayland(system, window, &display, &surface) == GRANIT_SUCCESS);
  REQUIRE(display != nullptr);
  REQUIRE(surface != nullptr);
  uint32_t xcb_window = UINT32_C(42);
  REQUIRE(granit_window_get_xcb(system, window, &display, &xcb_window) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(display == nullptr);
  CHECK(xcb_window == 0);

  REQUIRE(granit_window_destroy(system, window) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(system) == GRANIT_SUCCESS);
}
#endif
