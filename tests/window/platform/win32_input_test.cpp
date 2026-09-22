// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>
#include <granit/window/native.h>

#include <catch2/catch_all.hpp>

#include <string_view>

#include <windows.h>

TEST_CASE("Win32 Input 转换键盘、文本和指针消息", "[input][win32]") {
  granit_window_system_desc window_system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system window_system = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_system_create(&window_system_desc, &window_system) == GRANIT_SUCCESS);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = 0;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(window_system, &window_desc, &window) == GRANIT_SUCCESS);
  granit_window_native_win32 native = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  REQUIRE(granit_window_get_native_win32(window_system, window, &native) == GRANIT_SUCCESS);
  const auto hwnd = static_cast<HWND>(native.window);

  SendMessageW(hwnd, WM_KEYDOWN, 'A', LPARAM{0x001e0001});
  SendMessageW(hwnd, WM_KEYDOWN, 'A', LPARAM{0x401e0001});
  SendMessageW(hwnd, WM_KEYUP, 'A', LPARAM{0xc01e0001});
  SendMessageW(hwnd, WM_KEYDOWN, VK_LEFT, LPARAM{0x014b0001});
  SendMessageW(hwnd, WM_KEYUP, VK_LEFT, LPARAM{0xc14b0001});
  SendMessageW(hwnd, WM_CHAR, L'\u00e9', LPARAM{1});
  SendMessageW(hwnd, WM_CHAR, wchar_t{0xd83d}, LPARAM{1});
  SendMessageW(hwnd, WM_CHAR, wchar_t{0xde00}, LPARAM{1});
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(12, 18));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 18));
  SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(MK_LBUTTON, WHEEL_DELTA), MAKELPARAM(12, 18));
  REQUIRE(granit_window_system_process_events(window_system) == GRANIT_SUCCESS);

  granit_input_event event = GRANIT_INPUT_EVENT_INIT;
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_KEY);
  CHECK(event.data.key.physical_key == GRANIT_PHYSICAL_KEY_A);
  CHECK(event.data.key.logical_key == GRANIT_LOGICAL_KEY_NONE);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_PRESSED);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_REPEATED);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_RELEASED);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.physical_key == GRANIT_PHYSICAL_KEY_LEFT);
  CHECK(event.data.key.logical_key == GRANIT_LOGICAL_KEY_LEFT);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_PRESSED);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_RELEASED);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_TEXT);
  CHECK(event.data.text.length == 2);
  CHECK(std::string_view{event.data.text.utf8, event.data.text.length} == "é");
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_TEXT);
  CHECK(event.data.text.length == 4);
  CHECK(std::string_view{event.data.text.utf8, event.data.text.length} == "😀");
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_ENTERED);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_MOVED);
  CHECK(event.data.pointer_moved.x == 12.0F);
  CHECK(event.data.pointer_moved.y == 18.0F);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_BUTTON);
  CHECK(event.data.pointer_button.button == GRANIT_POINTER_PRIMARY_BIT);
  CHECK(event.data.pointer_button.pressed == 1);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_WHEEL);
  CHECK(event.data.pointer_wheel.delta_y == 1.0F);
  REQUIRE(granit_window_poll_input_event(window_system, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_LEFT);
  CHECK(granit_window_poll_input_event(window_system, &event) == GRANIT_ERROR_NOT_READY);

  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  REQUIRE(granit_window_get_keyboard_state(window_system, window, &keyboard) == GRANIT_SUCCESS);
  CHECK((keyboard.pressed_keys[GRANIT_PHYSICAL_KEY_A / 64] &
         (UINT64_C(1) << (GRANIT_PHYSICAL_KEY_A % 64))) == 0);
  SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, LPARAM{0x002a0001});
  SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
  REQUIRE(granit_window_get_keyboard_state(window_system, window, &keyboard) == GRANIT_SUCCESS);
  CHECK(keyboard.modifiers == 0);
  CHECK(keyboard.pressed_keys[GRANIT_PHYSICAL_KEY_LEFT_SHIFT / 64] == 0);

  REQUIRE(granit_window_destroy(window_system, window) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(window_system) == GRANIT_SUCCESS);
}
