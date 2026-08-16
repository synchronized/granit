// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>

#include <catch2/catch_all.hpp>

TEST_CASE("Window 组件骨架保持确定的失败与输出语义", "[window]") {
  granit_window_system system = UINT64_C(42);
  CHECK(granit_window_system_create(nullptr, &system) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(system == GRANIT_NULL_HANDLE);

  granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  CHECK(granit_window_system_create(&system_desc, &system) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(system == GRANIT_NULL_HANDLE);

  granit_window window = UINT64_C(42);
  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  CHECK(granit_window_create(system, &window_desc, &window) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(window == GRANIT_NULL_HANDLE);

  void* first = &system;
  void* second = &window;
  CHECK(granit_window_get_win32(system, window, &first, &second) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(first == nullptr);
  CHECK(second == nullptr);
}
