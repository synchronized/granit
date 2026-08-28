// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/xcb/input_adapter.h"

namespace granit::input::detail {
namespace {

constexpr std::uint32_t key_press = 2;
constexpr std::uint32_t key_release = 3;
constexpr std::uint32_t button_press = 4;
constexpr std::uint32_t button_release = 5;
constexpr std::uint32_t motion_notify = 6;
constexpr std::uint32_t enter_notify = 7;
constexpr std::uint32_t leave_notify = 8;

std::uint32_t physical_key(std::uint32_t keycode) noexcept {
  // Xorg 的核心 keycode 通常为 Linux evdev code + 8。
  switch (keycode) {
  case 9:
    return GRANIT_PHYSICAL_KEY_ESCAPE;
  case 10:
    return GRANIT_PHYSICAL_KEY_1;
  case 11:
    return GRANIT_PHYSICAL_KEY_2;
  case 12:
    return GRANIT_PHYSICAL_KEY_3;
  case 13:
    return GRANIT_PHYSICAL_KEY_4;
  case 14:
    return GRANIT_PHYSICAL_KEY_5;
  case 15:
    return GRANIT_PHYSICAL_KEY_6;
  case 16:
    return GRANIT_PHYSICAL_KEY_7;
  case 17:
    return GRANIT_PHYSICAL_KEY_8;
  case 18:
    return GRANIT_PHYSICAL_KEY_9;
  case 19:
    return GRANIT_PHYSICAL_KEY_0;
  case 22:
    return GRANIT_PHYSICAL_KEY_BACKSPACE;
  case 23:
    return GRANIT_PHYSICAL_KEY_TAB;
  case 24:
    return GRANIT_PHYSICAL_KEY_Q;
  case 25:
    return GRANIT_PHYSICAL_KEY_W;
  case 26:
    return GRANIT_PHYSICAL_KEY_E;
  case 27:
    return GRANIT_PHYSICAL_KEY_R;
  case 28:
    return GRANIT_PHYSICAL_KEY_T;
  case 29:
    return GRANIT_PHYSICAL_KEY_Y;
  case 30:
    return GRANIT_PHYSICAL_KEY_U;
  case 31:
    return GRANIT_PHYSICAL_KEY_I;
  case 32:
    return GRANIT_PHYSICAL_KEY_O;
  case 33:
    return GRANIT_PHYSICAL_KEY_P;
  case 36:
    return GRANIT_PHYSICAL_KEY_ENTER;
  case 37:
    return GRANIT_PHYSICAL_KEY_LEFT_CONTROL;
  case 38:
    return GRANIT_PHYSICAL_KEY_A;
  case 39:
    return GRANIT_PHYSICAL_KEY_S;
  case 40:
    return GRANIT_PHYSICAL_KEY_D;
  case 41:
    return GRANIT_PHYSICAL_KEY_F;
  case 42:
    return GRANIT_PHYSICAL_KEY_G;
  case 43:
    return GRANIT_PHYSICAL_KEY_H;
  case 44:
    return GRANIT_PHYSICAL_KEY_J;
  case 45:
    return GRANIT_PHYSICAL_KEY_K;
  case 46:
    return GRANIT_PHYSICAL_KEY_L;
  case 50:
    return GRANIT_PHYSICAL_KEY_LEFT_SHIFT;
  case 52:
    return GRANIT_PHYSICAL_KEY_Z;
  case 53:
    return GRANIT_PHYSICAL_KEY_X;
  case 54:
    return GRANIT_PHYSICAL_KEY_C;
  case 55:
    return GRANIT_PHYSICAL_KEY_V;
  case 56:
    return GRANIT_PHYSICAL_KEY_B;
  case 57:
    return GRANIT_PHYSICAL_KEY_N;
  case 58:
    return GRANIT_PHYSICAL_KEY_M;
  case 62:
    return GRANIT_PHYSICAL_KEY_RIGHT_SHIFT;
  case 64:
    return GRANIT_PHYSICAL_KEY_LEFT_ALT;
  case 65:
    return GRANIT_PHYSICAL_KEY_SPACE;
  case 67:
    return GRANIT_PHYSICAL_KEY_F1;
  case 68:
    return GRANIT_PHYSICAL_KEY_F2;
  case 69:
    return GRANIT_PHYSICAL_KEY_F3;
  case 70:
    return GRANIT_PHYSICAL_KEY_F4;
  case 71:
    return GRANIT_PHYSICAL_KEY_F5;
  case 72:
    return GRANIT_PHYSICAL_KEY_F6;
  case 73:
    return GRANIT_PHYSICAL_KEY_F7;
  case 74:
    return GRANIT_PHYSICAL_KEY_F8;
  case 75:
    return GRANIT_PHYSICAL_KEY_F9;
  case 76:
    return GRANIT_PHYSICAL_KEY_F10;
  case 95:
    return GRANIT_PHYSICAL_KEY_F11;
  case 96:
    return GRANIT_PHYSICAL_KEY_F12;
  case 105:
    return GRANIT_PHYSICAL_KEY_RIGHT_CONTROL;
  case 108:
    return GRANIT_PHYSICAL_KEY_RIGHT_ALT;
  case 110:
    return GRANIT_PHYSICAL_KEY_HOME;
  case 111:
    return GRANIT_PHYSICAL_KEY_UP;
  case 112:
    return GRANIT_PHYSICAL_KEY_PAGE_UP;
  case 113:
    return GRANIT_PHYSICAL_KEY_LEFT;
  case 114:
    return GRANIT_PHYSICAL_KEY_RIGHT;
  case 115:
    return GRANIT_PHYSICAL_KEY_END;
  case 116:
    return GRANIT_PHYSICAL_KEY_DOWN;
  case 117:
    return GRANIT_PHYSICAL_KEY_PAGE_DOWN;
  case 118:
    return GRANIT_PHYSICAL_KEY_INSERT;
  case 119:
    return GRANIT_PHYSICAL_KEY_DELETE;
  case 133:
    return GRANIT_PHYSICAL_KEY_LEFT_SUPER;
  case 134:
    return GRANIT_PHYSICAL_KEY_RIGHT_SUPER;
  default:
    return GRANIT_PHYSICAL_KEY_UNKNOWN;
  }
}

std::uint32_t logical_key(std::uint32_t physical) noexcept {
  switch (physical) {
  case GRANIT_PHYSICAL_KEY_ENTER:
    return GRANIT_LOGICAL_KEY_ENTER;
  case GRANIT_PHYSICAL_KEY_ESCAPE:
    return GRANIT_LOGICAL_KEY_ESCAPE;
  case GRANIT_PHYSICAL_KEY_BACKSPACE:
    return GRANIT_LOGICAL_KEY_BACKSPACE;
  case GRANIT_PHYSICAL_KEY_TAB:
    return GRANIT_LOGICAL_KEY_TAB;
  case GRANIT_PHYSICAL_KEY_SPACE:
    return GRANIT_LOGICAL_KEY_SPACE;
  case GRANIT_PHYSICAL_KEY_LEFT:
    return GRANIT_LOGICAL_KEY_LEFT;
  case GRANIT_PHYSICAL_KEY_RIGHT:
    return GRANIT_LOGICAL_KEY_RIGHT;
  case GRANIT_PHYSICAL_KEY_UP:
    return GRANIT_LOGICAL_KEY_UP;
  case GRANIT_PHYSICAL_KEY_DOWN:
    return GRANIT_LOGICAL_KEY_DOWN;
  case GRANIT_PHYSICAL_KEY_HOME:
    return GRANIT_LOGICAL_KEY_HOME;
  case GRANIT_PHYSICAL_KEY_END:
    return GRANIT_LOGICAL_KEY_END;
  case GRANIT_PHYSICAL_KEY_PAGE_UP:
    return GRANIT_LOGICAL_KEY_PAGE_UP;
  case GRANIT_PHYSICAL_KEY_PAGE_DOWN:
    return GRANIT_LOGICAL_KEY_PAGE_DOWN;
  case GRANIT_PHYSICAL_KEY_INSERT:
    return GRANIT_LOGICAL_KEY_INSERT;
  case GRANIT_PHYSICAL_KEY_DELETE:
    return GRANIT_LOGICAL_KEY_DELETE;
  default:
    if (physical >= GRANIT_PHYSICAL_KEY_F1 && physical <= GRANIT_PHYSICAL_KEY_F12)
      return GRANIT_LOGICAL_KEY_F1 + physical - GRANIT_PHYSICAL_KEY_F1;
    return GRANIT_LOGICAL_KEY_NONE;
  }
}

bool key_pressed(const granit_keyboard_state& state, std::uint32_t key) noexcept {
  return key < 256 && (state.pressed_keys[key / 64] & (UINT64_C(1) << (key % 64))) != 0;
}

std::uint32_t modifiers(const granit_keyboard_state& state, std::uint32_t native_state) noexcept {
  std::uint32_t result = 0;
  const auto add = [&](std::uint32_t key, std::uint32_t bit) {
    if (key_pressed(state, key))
      result |= bit;
  };
  add(GRANIT_PHYSICAL_KEY_LEFT_SHIFT, GRANIT_MODIFIER_LEFT_SHIFT_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_SHIFT, GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
  add(GRANIT_PHYSICAL_KEY_LEFT_CONTROL, GRANIT_MODIFIER_LEFT_CONTROL_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_CONTROL, GRANIT_MODIFIER_RIGHT_CONTROL_BIT);
  add(GRANIT_PHYSICAL_KEY_LEFT_ALT, GRANIT_MODIFIER_LEFT_ALT_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_ALT, GRANIT_MODIFIER_RIGHT_ALT_BIT);
  add(GRANIT_PHYSICAL_KEY_LEFT_SUPER, GRANIT_MODIFIER_LEFT_SUPER_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_SUPER, GRANIT_MODIFIER_RIGHT_SUPER_BIT);
  if ((native_state & UINT32_C(2)) != 0)
    result |= GRANIT_MODIFIER_CAPS_LOCK_BIT;
  if ((native_state & UINT32_C(16)) != 0)
    result |= GRANIT_MODIFIER_NUM_LOCK_BIT;
  return result;
}

std::uint32_t buttons(std::uint32_t state) noexcept {
  std::uint32_t result = 0;
  if ((state & (UINT32_C(1) << 8)) != 0)
    result |= GRANIT_POINTER_PRIMARY_BIT;
  if ((state & (UINT32_C(1) << 9)) != 0)
    result |= GRANIT_POINTER_MIDDLE_BIT;
  if ((state & (UINT32_C(1) << 10)) != 0)
    result |= GRANIT_POINTER_SECONDARY_BIT;
  return result;
}

std::uint32_t button_bit(std::uint32_t detail) noexcept {
  switch (detail) {
  case 1:
    return GRANIT_POINTER_PRIMARY_BIT;
  case 2:
    return GRANIT_POINTER_MIDDLE_BIT;
  case 3:
    return GRANIT_POINTER_SECONDARY_BIT;
  case 8:
    return GRANIT_POINTER_X1_BIT;
  case 9:
    return GRANIT_POINTER_X2_BIT;
  default:
    return 0;
  }
}

} // namespace

void handle_xcb_input(granit_window window, const xcb_input_event& event,
                      const xcb_input_sink& sink) {
  if (event.type == key_press || event.type == key_release) {
    auto& state = sink.keyboard(sink.user_data, window);
    const auto physical = physical_key(event.detail);
    const bool pressed = event.type == key_press;
    const bool repeated = pressed && key_pressed(state, physical);
    if (physical != GRANIT_PHYSICAL_KEY_UNKNOWN) {
      const auto mask = UINT64_C(1) << (physical % 64);
      if (pressed)
        state.pressed_keys[physical / 64] |= mask;
      else
        state.pressed_keys[physical / 64] &= ~mask;
    }
    state.modifiers = modifiers(state, event.state);
    granit_input_event_data data{};
    data.key.physical_key = physical;
    data.key.logical_key = logical_key(physical);
    data.key.modifiers = state.modifiers;
    data.key.action = pressed ? (repeated ? GRANIT_KEY_ACTION_REPEATED : GRANIT_KEY_ACTION_PRESSED)
                              : GRANIT_KEY_ACTION_RELEASED;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_KEY, data);
    return;
  }

  auto& pointer = sink.pointer(sink.user_data, window);
  const float previous_x = pointer.x;
  const float previous_y = pointer.y;
  pointer.x = static_cast<float>(event.x);
  pointer.y = static_cast<float>(event.y);
  if (event.type == enter_notify || event.type == leave_notify) {
    pointer.inside = event.type == enter_notify ? UINT32_C(1) : UINT32_C(0);
    sink.event(sink.user_data, window,
               event.type == enter_notify ? GRANIT_INPUT_EVENT_POINTER_ENTERED
                                          : GRANIT_INPUT_EVENT_POINTER_LEFT,
               {});
  } else if (event.type == motion_notify) {
    pointer.buttons =
        buttons(event.state) | (pointer.buttons & (GRANIT_POINTER_X1_BIT | GRANIT_POINTER_X2_BIT));
    pointer.inside = UINT32_C(1);
    granit_input_event_data data{};
    data.pointer_moved.x = pointer.x;
    data.pointer_moved.y = pointer.y;
    data.pointer_moved.delta_x = pointer.x - previous_x;
    data.pointer_moved.delta_y = pointer.y - previous_y;
    data.pointer_moved.buttons = pointer.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_MOVED, data);
  } else if (event.type == button_press && event.detail >= 4 && event.detail <= 7) {
    granit_input_event_data data{};
    data.pointer_wheel.x = pointer.x;
    data.pointer_wheel.y = pointer.y;
    data.pointer_wheel.delta_x = event.detail == 6 ? -1.0F : event.detail == 7 ? 1.0F : 0.0F;
    data.pointer_wheel.delta_y = event.detail == 4 ? 1.0F : event.detail == 5 ? -1.0F : 0.0F;
    data.pointer_wheel.buttons = pointer.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_WHEEL, data);
  } else if (event.type == button_press || event.type == button_release) {
    const auto bit = button_bit(event.detail);
    if (bit == 0)
      return;
    pointer.buttons =
        buttons(event.state) | (pointer.buttons & (GRANIT_POINTER_X1_BIT | GRANIT_POINTER_X2_BIT));
    const bool pressed = event.type == button_press;
    if (pressed)
      pointer.buttons |= bit;
    else
      pointer.buttons &= ~bit;
    granit_input_event_data data{};
    data.pointer_button.x = pointer.x;
    data.pointer_button.y = pointer.y;
    data.pointer_button.button = bit;
    data.pointer_button.pressed = pressed ? UINT32_C(1) : UINT32_C(0);
    data.pointer_button.buttons = pointer.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_BUTTON, data);
  }
}

} // namespace granit::input::detail
