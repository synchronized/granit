// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_INPUT_HPP_
#define GRANIT_WINDOW_INPUT_HPP_

#include <cstdint>
#include <cstring>
#include <type_traits>

#include <granit/window/input.h>

namespace granit {

enum class input_event_type : std::uint32_t {
  none = 0,
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

enum class physical_key : std::uint32_t {
  unknown = GRANIT_PHYSICAL_KEY_UNKNOWN,
  a = GRANIT_PHYSICAL_KEY_A,
  b = GRANIT_PHYSICAL_KEY_B,
  c = GRANIT_PHYSICAL_KEY_C,
  d = GRANIT_PHYSICAL_KEY_D,
  e = GRANIT_PHYSICAL_KEY_E,
  f = GRANIT_PHYSICAL_KEY_F,
  g = GRANIT_PHYSICAL_KEY_G,
  h = GRANIT_PHYSICAL_KEY_H,
  i = GRANIT_PHYSICAL_KEY_I,
  j = GRANIT_PHYSICAL_KEY_J,
  k = GRANIT_PHYSICAL_KEY_K,
  l = GRANIT_PHYSICAL_KEY_L,
  m = GRANIT_PHYSICAL_KEY_M,
  n = GRANIT_PHYSICAL_KEY_N,
  o = GRANIT_PHYSICAL_KEY_O,
  p = GRANIT_PHYSICAL_KEY_P,
  q = GRANIT_PHYSICAL_KEY_Q,
  r = GRANIT_PHYSICAL_KEY_R,
  s = GRANIT_PHYSICAL_KEY_S,
  t = GRANIT_PHYSICAL_KEY_T,
  u = GRANIT_PHYSICAL_KEY_U,
  v = GRANIT_PHYSICAL_KEY_V,
  w = GRANIT_PHYSICAL_KEY_W,
  x = GRANIT_PHYSICAL_KEY_X,
  y = GRANIT_PHYSICAL_KEY_Y,
  z = GRANIT_PHYSICAL_KEY_Z,
  digit_1 = GRANIT_PHYSICAL_KEY_1,
  digit_2 = GRANIT_PHYSICAL_KEY_2,
  digit_3 = GRANIT_PHYSICAL_KEY_3,
  digit_4 = GRANIT_PHYSICAL_KEY_4,
  digit_5 = GRANIT_PHYSICAL_KEY_5,
  digit_6 = GRANIT_PHYSICAL_KEY_6,
  digit_7 = GRANIT_PHYSICAL_KEY_7,
  digit_8 = GRANIT_PHYSICAL_KEY_8,
  digit_9 = GRANIT_PHYSICAL_KEY_9,
  digit_0 = GRANIT_PHYSICAL_KEY_0,
  enter = GRANIT_PHYSICAL_KEY_ENTER,
  escape = GRANIT_PHYSICAL_KEY_ESCAPE,
  backspace = GRANIT_PHYSICAL_KEY_BACKSPACE,
  tab = GRANIT_PHYSICAL_KEY_TAB,
  space = GRANIT_PHYSICAL_KEY_SPACE,
  f1 = GRANIT_PHYSICAL_KEY_F1,
  f2 = GRANIT_PHYSICAL_KEY_F2,
  f3 = GRANIT_PHYSICAL_KEY_F3,
  f4 = GRANIT_PHYSICAL_KEY_F4,
  f5 = GRANIT_PHYSICAL_KEY_F5,
  f6 = GRANIT_PHYSICAL_KEY_F6,
  f7 = GRANIT_PHYSICAL_KEY_F7,
  f8 = GRANIT_PHYSICAL_KEY_F8,
  f9 = GRANIT_PHYSICAL_KEY_F9,
  f10 = GRANIT_PHYSICAL_KEY_F10,
  f11 = GRANIT_PHYSICAL_KEY_F11,
  f12 = GRANIT_PHYSICAL_KEY_F12,
  insert = GRANIT_PHYSICAL_KEY_INSERT,
  home = GRANIT_PHYSICAL_KEY_HOME,
  page_up = GRANIT_PHYSICAL_KEY_PAGE_UP,
  delete_key = GRANIT_PHYSICAL_KEY_DELETE,
  end = GRANIT_PHYSICAL_KEY_END,
  page_down = GRANIT_PHYSICAL_KEY_PAGE_DOWN,
  right = GRANIT_PHYSICAL_KEY_RIGHT,
  left = GRANIT_PHYSICAL_KEY_LEFT,
  down = GRANIT_PHYSICAL_KEY_DOWN,
  up = GRANIT_PHYSICAL_KEY_UP,
  left_control = GRANIT_PHYSICAL_KEY_LEFT_CONTROL,
  left_shift = GRANIT_PHYSICAL_KEY_LEFT_SHIFT,
  left_alt = GRANIT_PHYSICAL_KEY_LEFT_ALT,
  left_super = GRANIT_PHYSICAL_KEY_LEFT_SUPER,
  right_control = GRANIT_PHYSICAL_KEY_RIGHT_CONTROL,
  right_shift = GRANIT_PHYSICAL_KEY_RIGHT_SHIFT,
  right_alt = GRANIT_PHYSICAL_KEY_RIGHT_ALT,
  right_super = GRANIT_PHYSICAL_KEY_RIGHT_SUPER,
};

enum class logical_key : std::uint32_t {
  none = GRANIT_LOGICAL_KEY_NONE,
  enter = GRANIT_LOGICAL_KEY_ENTER,
  escape = GRANIT_LOGICAL_KEY_ESCAPE,
  backspace = GRANIT_LOGICAL_KEY_BACKSPACE,
  tab = GRANIT_LOGICAL_KEY_TAB,
  space = GRANIT_LOGICAL_KEY_SPACE,
  left = GRANIT_LOGICAL_KEY_LEFT,
  right = GRANIT_LOGICAL_KEY_RIGHT,
  up = GRANIT_LOGICAL_KEY_UP,
  down = GRANIT_LOGICAL_KEY_DOWN,
  home = GRANIT_LOGICAL_KEY_HOME,
  end = GRANIT_LOGICAL_KEY_END,
  page_up = GRANIT_LOGICAL_KEY_PAGE_UP,
  page_down = GRANIT_LOGICAL_KEY_PAGE_DOWN,
  insert = GRANIT_LOGICAL_KEY_INSERT,
  delete_key = GRANIT_LOGICAL_KEY_DELETE,
  f1 = GRANIT_LOGICAL_KEY_F1,
  f2 = GRANIT_LOGICAL_KEY_F2,
  f3 = GRANIT_LOGICAL_KEY_F3,
  f4 = GRANIT_LOGICAL_KEY_F4,
  f5 = GRANIT_LOGICAL_KEY_F5,
  f6 = GRANIT_LOGICAL_KEY_F6,
  f7 = GRANIT_LOGICAL_KEY_F7,
  f8 = GRANIT_LOGICAL_KEY_F8,
  f9 = GRANIT_LOGICAL_KEY_F9,
  f10 = GRANIT_LOGICAL_KEY_F10,
  f11 = GRANIT_LOGICAL_KEY_F11,
  f12 = GRANIT_LOGICAL_KEY_F12,
};

struct key_input_event {
  physical_key physical{physical_key::unknown};
  logical_key logical{logical_key::none};
  std::uint32_t modifiers{};
  key_action action{key_action::released};
};

struct text_input_event {
  std::uint32_t length{};
  char utf8[GRANIT_INPUT_TEXT_CAPACITY]{};
  std::uint8_t reserved[12]{};
};

struct pointer_motion_event {
  float x{};
  float y{};
  float delta_x{};
  float delta_y{};
  std::uint32_t buttons{};
  std::uint32_t reserved[11]{};
};

struct pointer_button_event {
  float x{};
  float y{};
  std::uint32_t button{};
  std::uint32_t pressed{};
  std::uint32_t buttons{};
  std::uint32_t reserved[11]{};
};

union input_event_data {
  key_input_event key;
  text_input_event text;
  pointer_motion_event pointer_moved;
  pointer_button_event pointer_button;
  pointer_motion_event pointer_wheel;
  std::uint8_t reserved[64]{};
};

struct input_event {
  input_event_type type{};
  granit_window window{GRANIT_NULL_HANDLE};
  std::uint64_t timestamp_ns{};
  input_event_data data{};
};

using keyboard_state = granit_keyboard_state;
using pointer_state = granit_pointer_state;

static_assert(std::is_trivially_copyable_v<input_event_data>);
static_assert(sizeof(key_input_event) == sizeof(granit_input_event_data{}.key));
static_assert(sizeof(text_input_event) == sizeof(granit_input_event_data{}.text));
static_assert(sizeof(pointer_motion_event) == sizeof(granit_input_event_data{}.pointer_moved));
static_assert(sizeof(pointer_button_event) == sizeof(granit_input_event_data{}.pointer_button));
static_assert(sizeof(input_event_data) == sizeof(granit_input_event_data));

namespace detail {

[[nodiscard]] inline input_event from_native(const granit_input_event& native) noexcept {
  input_event event{.type = static_cast<input_event_type>(native.type),
                    .window = native.window,
                    .timestamp_ns = native.timestamp_ns};
  switch (event.type) {
  case input_event_type::key:
    event.data.key.physical = static_cast<physical_key>(native.data.key.physical_key);
    event.data.key.logical = static_cast<logical_key>(native.data.key.logical_key);
    event.data.key.modifiers = native.data.key.modifiers;
    event.data.key.action = static_cast<key_action>(native.data.key.action);
    break;
  case input_event_type::text:
    event.data.text.length = native.data.text.length;
    std::memcpy(event.data.text.utf8, native.data.text.utf8, sizeof(event.data.text.utf8));
    break;
  case input_event_type::pointer_moved:
    event.data.pointer_moved = {.x = native.data.pointer_moved.x,
                                .y = native.data.pointer_moved.y,
                                .delta_x = native.data.pointer_moved.delta_x,
                                .delta_y = native.data.pointer_moved.delta_y,
                                .buttons = native.data.pointer_moved.buttons};
    break;
  case input_event_type::pointer_button:
    event.data.pointer_button = {.x = native.data.pointer_button.x,
                                 .y = native.data.pointer_button.y,
                                 .button = native.data.pointer_button.button,
                                 .pressed = native.data.pointer_button.pressed,
                                 .buttons = native.data.pointer_button.buttons};
    break;
  case input_event_type::pointer_wheel:
    event.data.pointer_wheel = {.x = native.data.pointer_wheel.x,
                                .y = native.data.pointer_wheel.y,
                                .delta_x = native.data.pointer_wheel.delta_x,
                                .delta_y = native.data.pointer_wheel.delta_y,
                                .buttons = native.data.pointer_wheel.buttons};
    break;
  case input_event_type::none:
  case input_event_type::pointer_entered:
  case input_event_type::pointer_left:
    break;
  }
  return event;
}

} // namespace detail

[[nodiscard]] inline bool key_is_pressed(const keyboard_state& state,
                                         std::uint32_t physical_key) noexcept {
  return physical_key < 256 &&
         (state.pressed_keys[physical_key / 64] & (UINT64_C(1) << (physical_key % 64))) != 0;
}

[[nodiscard]] inline bool key_is_pressed(const keyboard_state& state, physical_key key) noexcept {
  return key_is_pressed(state, static_cast<std::uint32_t>(key));
}

} // namespace granit

#endif
