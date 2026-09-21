// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>
#include <granit/window.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <cstring>

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

TEST_CASE("Input 版本化输出不写越调用方容量", "[input][abi]") {
  check_versioned_output<granit_input_event, GRANIT_INPUT_EVENT_VERSION_1_SIZE>(
      [](granit_input_event* output) { return granit_window_poll_input_event(UINT64_MAX, output); },
      GRANIT_ERROR_INVALID_HANDLE);
  check_versioned_output<granit_keyboard_state, GRANIT_KEYBOARD_STATE_VERSION_1_SIZE>(
      [](granit_keyboard_state* output) {
        return granit_window_get_keyboard_state(UINT64_MAX, UINT64_C(1), output);
      },
      GRANIT_ERROR_INVALID_HANDLE);
  check_versioned_output<granit_pointer_state, GRANIT_POINTER_STATE_VERSION_1_SIZE>(
      [](granit_pointer_state* output) {
        return granit_window_get_pointer_state(UINT64_MAX, UINT64_C(1), output);
      },
      GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Window System 在当前平台直接拥有输入状态和事件队列", "[input][contract]") {
  granit_window_system_desc window_system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system window_system = GRANIT_NULL_HANDLE;
  const auto system_result = granit_window_system_create(&window_system_desc, &window_system);
  if (system_result == GRANIT_ERROR_BACKEND_UNAVAILABLE)
    SKIP("当前环境没有可用的 Window 后端");
  REQUIRE(system_result == GRANIT_SUCCESS);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = 0;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(window_system, &window_desc, &window) == GRANIT_SUCCESS);

  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  granit_pointer_state pointer = GRANIT_POINTER_STATE_INIT;
  CHECK(granit_window_get_keyboard_state(window_system, window, &keyboard) == GRANIT_SUCCESS);
  CHECK(granit_window_get_pointer_state(window_system, window, &pointer) == GRANIT_SUCCESS);
  CHECK(keyboard.struct_size == sizeof(granit_keyboard_state));
  CHECK(pointer.struct_size == sizeof(granit_pointer_state));

  granit_input_event input_event = GRANIT_INPUT_EVENT_INIT;
  CHECK(granit_window_poll_input_event(window_system, &input_event) == GRANIT_ERROR_NOT_READY);

  REQUIRE(granit_window_destroy(window_system, window) == GRANIT_SUCCESS);
  keyboard.modifiers = UINT32_MAX;
  CHECK(granit_window_get_keyboard_state(window_system, window, &keyboard) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(keyboard.modifiers == 0);
  REQUIRE(granit_window_system_destroy(window_system) == GRANIT_SUCCESS);
}

TEST_CASE("Window创建把空Window System归类为无效句柄", "[window][contract]") {
  CHECK(granit_window_system_process_events(UINT64_MAX) == GRANIT_ERROR_INVALID_HANDLE);
  granit_surface surface = UINT64_C(42);
  CHECK(granit_window_create_surface(UINT64_MAX, UINT64_C(1), UINT64_C(1), &surface) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(surface == GRANIT_NULL_HANDLE);
  CHECK(granit_window_create_surface(UINT64_MAX, UINT64_C(1), UINT64_C(1), nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

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
