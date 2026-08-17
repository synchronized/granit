// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INPUT_INPUT_HPP_
#define GRANIT_INPUT_INPUT_HPP_

#include <cstdint>

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

[[nodiscard]] inline bool key_is_pressed(const keyboard_state& state,
                                         std::uint32_t physical_key) noexcept {
  return physical_key < 256 &&
         (state.pressed_keys[physical_key / 64] & (UINT64_C(1) << (physical_key % 64))) != 0;
}

} // namespace granit

#endif
