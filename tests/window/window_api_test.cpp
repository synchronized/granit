// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>

#include <catch2/catch_all.hpp>

#if defined(_WIN32)
#include <windows.h>
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
  window_desc.flags = 0;
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
#else
  CHECK(granit_window_system_create(&system_desc, &system) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(system == GRANIT_NULL_HANDLE);
#endif
}
