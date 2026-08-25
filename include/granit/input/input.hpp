// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INPUT_INPUT_HPP_
#define GRANIT_INPUT_INPUT_HPP_

#include <cstdint>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/input/input.h>

namespace granit {

enum class input_event_type : std::uint32_t {
  key = GRANIT_INPUT_EVENT_KEY,
  text = GRANIT_INPUT_EVENT_TEXT,
  pointer_moved = GRANIT_INPUT_EVENT_POINTER_MOVED,
  pointer_button = GRANIT_INPUT_EVENT_POINTER_BUTTON,
  pointer_wheel = GRANIT_INPUT_EVENT_POINTER_WHEEL,
  pointer_entered = GRANIT_INPUT_EVENT_POINTER_ENTERED,
  pointer_left = GRANIT_INPUT_EVENT_POINTER_LEFT,
};

enum class key_action : std::uint32_t {
  released = GRANIT_KEY_ACTION_RELEASED,
  pressed = GRANIT_KEY_ACTION_PRESSED,
  repeated = GRANIT_KEY_ACTION_REPEATED,
};

using input_event = granit_input_event;
using keyboard_state = granit_keyboard_state;
using pointer_state = granit_pointer_state;

class input_system {
public:
  input_system() = default;
  ~input_system() { static_cast<void>(reset()); }
  input_system(const input_system&) = delete;
  input_system& operator=(const input_system&) = delete;
  input_system(input_system&& other) noexcept
      : handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  input_system& operator=(input_system&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_window_system window_system) noexcept {
    if (valid())
      return result::invalid_argument;
    if (window_system == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_input_system_desc desc{sizeof(granit_input_system_desc), window_system, 0, 0};
    return from_native(granit_input_system_create(&desc, &handle_));
  }
  [[nodiscard]] result poll(input_event& event) noexcept {
    return from_native(granit_input_poll_event(handle_, &event));
  }
  [[nodiscard]] result keyboard(granit_window window, keyboard_state& state) const noexcept {
    return from_native(granit_input_get_keyboard_state(handle_, window, &state));
  }
  [[nodiscard]] result pointer(granit_window window, pointer_state& state) const noexcept {
    return from_native(granit_input_get_pointer_state(handle_, window, &state));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto value = granit_input_system_destroy(handle_);
    if (value == GRANIT_SUCCESS)
      handle_ = GRANIT_NULL_HANDLE;
    return from_native(value);
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_input_system native_handle() const noexcept { return handle_; }

private:
  granit_input_system handle_{GRANIT_NULL_HANDLE};
};

[[nodiscard]] inline bool key_is_pressed(const keyboard_state& state,
                                         std::uint32_t physical_key) noexcept {
  return physical_key < 256 &&
         (state.pressed_keys[physical_key / 64] & (UINT64_C(1) << (physical_key % 64))) != 0;
}

} // namespace granit

#endif
