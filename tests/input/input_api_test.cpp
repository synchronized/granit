// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input.h>

#include <catch2/catch_all.hpp>

TEST_CASE("Input System 附着 Window System 并保持窗口事件队列", "[input]") {
#if defined(_WIN32)
  granit_window_system_desc window_system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system window_system = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_system_create(&window_system_desc, &window_system) == GRANIT_SUCCESS);

  granit_input_system_desc input_desc = GRANIT_INPUT_SYSTEM_DESC_INIT;
  input_desc.window_system = window_system;
  granit_input_system input = GRANIT_NULL_HANDLE;
  REQUIRE(granit_input_system_create(&input_desc, &input) == GRANIT_SUCCESS);
  granit_input_system duplicate = GRANIT_NULL_HANDLE;
  CHECK(granit_input_system_create(&input_desc, &duplicate) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = 0;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(window_system, &window_desc, &window) == GRANIT_SUCCESS);

  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  granit_pointer_state pointer = GRANIT_POINTER_STATE_INIT;
  CHECK(granit_input_get_keyboard_state(input, window, &keyboard) == GRANIT_SUCCESS);
  CHECK(granit_input_get_pointer_state(input, window, &pointer) == GRANIT_SUCCESS);
  CHECK(keyboard.struct_size == sizeof(granit_keyboard_state));
  CHECK(pointer.struct_size == sizeof(granit_pointer_state));

  granit_input_event input_event = GRANIT_INPUT_EVENT_INIT;
  CHECK(granit_input_poll_event(input, &input_event) == GRANIT_ERROR_NOT_READY);

  REQUIRE(granit_window_destroy(window_system, window) == GRANIT_SUCCESS);
  keyboard.modifiers = UINT32_MAX;
  CHECK(granit_input_get_keyboard_state(input, window, &keyboard) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(keyboard.modifiers == 0);
  REQUIRE(granit_input_system_destroy(input) == GRANIT_SUCCESS);
  CHECK(granit_input_system_destroy(input) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_window_system_destroy(window_system) == GRANIT_SUCCESS);
#else
  SUCCEED("平台运行测试由对应 Window 后端环境覆盖");
#endif
}
