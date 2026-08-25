// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input.h>
#include <granit/input.hpp>
#include <granit/window.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

template <typename T, std::size_t VersionSize>
void check_versioned_output(granit_result (*call)(T*), granit_result expected) {
  alignas(T) std::array<std::byte, VersionSize + 8> storage{};
  std::memset(storage.data(), 0x5a, storage.size());
  auto* output = reinterpret_cast<T*>(storage.data());
  const auto capacity = static_cast<uint32_t>(VersionSize);
  std::memcpy(&output->struct_size, &capacity, sizeof(capacity));
  CHECK(call(output) == expected);
  CHECK(output->struct_size == VersionSize);
  for (std::size_t index = VersionSize; index < storage.size(); ++index)
    CHECK(storage[index] == std::byte{0x5a});
}

} // namespace

TEST_CASE("Input创建把空Window System归类为无效句柄", "[input][contract]") {
  const granit_input_system_desc desc{GRANIT_INPUT_SYSTEM_DESC_VERSION_1_SIZE,
                                      GRANIT_NULL_HANDLE, 0, 0};
  granit_input_system handle = UINT64_C(1);
  CHECK(granit_input_system_create(&desc, &handle) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(handle == GRANIT_NULL_HANDLE);

  granit::input_system input;
  CHECK(input.initialize(GRANIT_NULL_HANDLE) == granit::result::invalid_handle);
}

#if defined(_WIN32)
TEST_CASE("Input包装在底层句柄失效后清空本地状态", "[input][lifetime]") {
  granit::window_system windows;
  REQUIRE(windows.initialize({.backend = granit::window_backend::win32}) ==
          granit::result::success);
  granit::input_system input;
  REQUIRE(input.initialize(windows.native_handle()) == granit::result::success);
  REQUIRE(granit_input_system_destroy(input.native_handle()) == GRANIT_SUCCESS);
  CHECK(input.reset() == granit::result::invalid_handle);
  CHECK_FALSE(input.valid());
}
#endif

TEST_CASE("Input 版本化输出不写越调用方容量", "[input][abi]") {
  check_versioned_output<granit_input_event, GRANIT_INPUT_EVENT_VERSION_1_SIZE>(
      [](granit_input_event* output) { return granit_input_poll_event(UINT64_C(1), output); },
      GRANIT_ERROR_INVALID_HANDLE);
  check_versioned_output<granit_keyboard_state, GRANIT_KEYBOARD_STATE_VERSION_1_SIZE>(
      [](granit_keyboard_state* output) {
        return granit_input_get_keyboard_state(UINT64_C(1), UINT64_C(1), output);
      },
      GRANIT_ERROR_INVALID_HANDLE);
  check_versioned_output<granit_pointer_state, GRANIT_POINTER_STATE_VERSION_1_SIZE>(
      [](granit_pointer_state* output) {
        return granit_input_get_pointer_state(UINT64_C(1), UINT64_C(1), output);
      },
      GRANIT_ERROR_INVALID_HANDLE);
}

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

TEST_CASE("Win32 Input 转换键盘、文本和指针消息", "[input][win32]") {
#if defined(_WIN32)
  granit_window_system_desc window_system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system window_system = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_system_create(&window_system_desc, &window_system) == GRANIT_SUCCESS);

  granit_input_system_desc input_desc = GRANIT_INPUT_SYSTEM_DESC_INIT;
  input_desc.window_system = window_system;
  granit_input_system input = GRANIT_NULL_HANDLE;
  REQUIRE(granit_input_system_create(&input_desc, &input) == GRANIT_SUCCESS);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = 0;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(window_system, &window_desc, &window) == GRANIT_SUCCESS);
  void* instance = nullptr;
  void* native_window = nullptr;
  REQUIRE(granit_window_get_win32(window_system, window, &instance, &native_window) ==
          GRANIT_SUCCESS);
  const auto hwnd = static_cast<HWND>(native_window);

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

  granit_input_event event = GRANIT_INPUT_EVENT_INIT;
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_KEY);
  CHECK(event.data.key.physical_key == GRANIT_PHYSICAL_KEY_A);
  CHECK(event.data.key.logical_key == GRANIT_LOGICAL_KEY_NONE);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_PRESSED);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_REPEATED);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_RELEASED);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.physical_key == GRANIT_PHYSICAL_KEY_LEFT);
  CHECK(event.data.key.logical_key == GRANIT_LOGICAL_KEY_LEFT);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_PRESSED);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.data.key.action == GRANIT_KEY_ACTION_RELEASED);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_TEXT);
  CHECK(event.data.text.length == 2);
  CHECK(std::string_view{event.data.text.utf8, event.data.text.length} == "é");
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_TEXT);
  CHECK(event.data.text.length == 4);
  CHECK(std::string_view{event.data.text.utf8, event.data.text.length} == "😀");
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_ENTERED);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_MOVED);
  CHECK(event.data.pointer_moved.x == 12.0F);
  CHECK(event.data.pointer_moved.y == 18.0F);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_BUTTON);
  CHECK(event.data.pointer_button.button == GRANIT_POINTER_PRIMARY_BIT);
  CHECK(event.data.pointer_button.pressed == 1);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_WHEEL);
  CHECK(event.data.pointer_wheel.delta_y == 1.0F);
  REQUIRE(granit_input_poll_event(input, &event) == GRANIT_SUCCESS);
  CHECK(event.type == GRANIT_INPUT_EVENT_POINTER_LEFT);
  CHECK(granit_input_poll_event(input, &event) == GRANIT_ERROR_NOT_READY);

  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  REQUIRE(granit_input_get_keyboard_state(input, window, &keyboard) == GRANIT_SUCCESS);
  CHECK((keyboard.pressed_keys[GRANIT_PHYSICAL_KEY_A / 64] &
         (UINT64_C(1) << (GRANIT_PHYSICAL_KEY_A % 64))) == 0);
  SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, LPARAM{0x002a0001});
  SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
  REQUIRE(granit_input_get_keyboard_state(input, window, &keyboard) == GRANIT_SUCCESS);
  CHECK(keyboard.modifiers == 0);
  CHECK(keyboard.pressed_keys[GRANIT_PHYSICAL_KEY_LEFT_SHIFT / 64] == 0);

  REQUIRE(granit_window_destroy(window_system, window) == GRANIT_SUCCESS);
  REQUIRE(granit_input_system_destroy(input) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(window_system) == GRANIT_SUCCESS);
#else
  SUCCEED("仅在 Win32 验证平台消息转换");
#endif
}
