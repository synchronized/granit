// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "wayland_input_adapter.h"

#include <xkbcommon/xkbcommon.h>

#include <new>
#include <string>

namespace granit::input::detail {
namespace {

std::uint32_t physical_key(std::uint32_t key) noexcept {
  switch (key) {
  case 1:
    return GRANIT_PHYSICAL_KEY_ESCAPE;
  case 2:
    return GRANIT_PHYSICAL_KEY_1;
  case 3:
    return GRANIT_PHYSICAL_KEY_2;
  case 4:
    return GRANIT_PHYSICAL_KEY_3;
  case 5:
    return GRANIT_PHYSICAL_KEY_4;
  case 6:
    return GRANIT_PHYSICAL_KEY_5;
  case 7:
    return GRANIT_PHYSICAL_KEY_6;
  case 8:
    return GRANIT_PHYSICAL_KEY_7;
  case 9:
    return GRANIT_PHYSICAL_KEY_8;
  case 10:
    return GRANIT_PHYSICAL_KEY_9;
  case 11:
    return GRANIT_PHYSICAL_KEY_0;
  case 14:
    return GRANIT_PHYSICAL_KEY_BACKSPACE;
  case 15:
    return GRANIT_PHYSICAL_KEY_TAB;
  case 16:
    return GRANIT_PHYSICAL_KEY_Q;
  case 17:
    return GRANIT_PHYSICAL_KEY_W;
  case 18:
    return GRANIT_PHYSICAL_KEY_E;
  case 19:
    return GRANIT_PHYSICAL_KEY_R;
  case 20:
    return GRANIT_PHYSICAL_KEY_T;
  case 21:
    return GRANIT_PHYSICAL_KEY_Y;
  case 22:
    return GRANIT_PHYSICAL_KEY_U;
  case 23:
    return GRANIT_PHYSICAL_KEY_I;
  case 24:
    return GRANIT_PHYSICAL_KEY_O;
  case 25:
    return GRANIT_PHYSICAL_KEY_P;
  case 28:
    return GRANIT_PHYSICAL_KEY_ENTER;
  case 29:
    return GRANIT_PHYSICAL_KEY_LEFT_CONTROL;
  case 30:
    return GRANIT_PHYSICAL_KEY_A;
  case 31:
    return GRANIT_PHYSICAL_KEY_S;
  case 32:
    return GRANIT_PHYSICAL_KEY_D;
  case 33:
    return GRANIT_PHYSICAL_KEY_F;
  case 34:
    return GRANIT_PHYSICAL_KEY_G;
  case 35:
    return GRANIT_PHYSICAL_KEY_H;
  case 36:
    return GRANIT_PHYSICAL_KEY_J;
  case 37:
    return GRANIT_PHYSICAL_KEY_K;
  case 38:
    return GRANIT_PHYSICAL_KEY_L;
  case 42:
    return GRANIT_PHYSICAL_KEY_LEFT_SHIFT;
  case 44:
    return GRANIT_PHYSICAL_KEY_Z;
  case 45:
    return GRANIT_PHYSICAL_KEY_X;
  case 46:
    return GRANIT_PHYSICAL_KEY_C;
  case 47:
    return GRANIT_PHYSICAL_KEY_V;
  case 48:
    return GRANIT_PHYSICAL_KEY_B;
  case 49:
    return GRANIT_PHYSICAL_KEY_N;
  case 50:
    return GRANIT_PHYSICAL_KEY_M;
  case 54:
    return GRANIT_PHYSICAL_KEY_RIGHT_SHIFT;
  case 56:
    return GRANIT_PHYSICAL_KEY_LEFT_ALT;
  case 57:
    return GRANIT_PHYSICAL_KEY_SPACE;
  case 59:
    return GRANIT_PHYSICAL_KEY_F1;
  case 60:
    return GRANIT_PHYSICAL_KEY_F2;
  case 61:
    return GRANIT_PHYSICAL_KEY_F3;
  case 62:
    return GRANIT_PHYSICAL_KEY_F4;
  case 63:
    return GRANIT_PHYSICAL_KEY_F5;
  case 64:
    return GRANIT_PHYSICAL_KEY_F6;
  case 65:
    return GRANIT_PHYSICAL_KEY_F7;
  case 66:
    return GRANIT_PHYSICAL_KEY_F8;
  case 67:
    return GRANIT_PHYSICAL_KEY_F9;
  case 68:
    return GRANIT_PHYSICAL_KEY_F10;
  case 87:
    return GRANIT_PHYSICAL_KEY_F11;
  case 88:
    return GRANIT_PHYSICAL_KEY_F12;
  case 97:
    return GRANIT_PHYSICAL_KEY_RIGHT_CONTROL;
  case 100:
    return GRANIT_PHYSICAL_KEY_RIGHT_ALT;
  case 102:
    return GRANIT_PHYSICAL_KEY_HOME;
  case 103:
    return GRANIT_PHYSICAL_KEY_UP;
  case 104:
    return GRANIT_PHYSICAL_KEY_PAGE_UP;
  case 105:
    return GRANIT_PHYSICAL_KEY_LEFT;
  case 106:
    return GRANIT_PHYSICAL_KEY_RIGHT;
  case 107:
    return GRANIT_PHYSICAL_KEY_END;
  case 108:
    return GRANIT_PHYSICAL_KEY_DOWN;
  case 109:
    return GRANIT_PHYSICAL_KEY_PAGE_DOWN;
  case 110:
    return GRANIT_PHYSICAL_KEY_INSERT;
  case 111:
    return GRANIT_PHYSICAL_KEY_DELETE;
  case 125:
    return GRANIT_PHYSICAL_KEY_LEFT_SUPER;
  case 126:
    return GRANIT_PHYSICAL_KEY_RIGHT_SUPER;
  default:
    return GRANIT_PHYSICAL_KEY_UNKNOWN;
  }
}

std::uint32_t logical_key(xkb_keysym_t symbol) noexcept {
  switch (symbol) {
  case XKB_KEY_Return:
    return GRANIT_LOGICAL_KEY_ENTER;
  case XKB_KEY_Escape:
    return GRANIT_LOGICAL_KEY_ESCAPE;
  case XKB_KEY_BackSpace:
    return GRANIT_LOGICAL_KEY_BACKSPACE;
  case XKB_KEY_Tab:
    return GRANIT_LOGICAL_KEY_TAB;
  case XKB_KEY_space:
    return GRANIT_LOGICAL_KEY_SPACE;
  case XKB_KEY_Left:
    return GRANIT_LOGICAL_KEY_LEFT;
  case XKB_KEY_Right:
    return GRANIT_LOGICAL_KEY_RIGHT;
  case XKB_KEY_Up:
    return GRANIT_LOGICAL_KEY_UP;
  case XKB_KEY_Down:
    return GRANIT_LOGICAL_KEY_DOWN;
  case XKB_KEY_Home:
    return GRANIT_LOGICAL_KEY_HOME;
  case XKB_KEY_End:
    return GRANIT_LOGICAL_KEY_END;
  case XKB_KEY_Page_Up:
    return GRANIT_LOGICAL_KEY_PAGE_UP;
  case XKB_KEY_Page_Down:
    return GRANIT_LOGICAL_KEY_PAGE_DOWN;
  case XKB_KEY_Insert:
    return GRANIT_LOGICAL_KEY_INSERT;
  case XKB_KEY_Delete:
    return GRANIT_LOGICAL_KEY_DELETE;
  default:
    if (symbol >= XKB_KEY_F1 && symbol <= XKB_KEY_F12)
      return GRANIT_LOGICAL_KEY_F1 + symbol - XKB_KEY_F1;
    return GRANIT_LOGICAL_KEY_NONE;
  }
}

void set_key(granit_keyboard_state& state, std::uint32_t key, bool pressed) noexcept {
  if (key == GRANIT_PHYSICAL_KEY_UNKNOWN || key >= 256)
    return;
  const auto mask = UINT64_C(1) << (key % 64);
  if (pressed)
    state.pressed_keys[key / 64] |= mask;
  else
    state.pressed_keys[key / 64] &= ~mask;
}

bool key_is_pressed(const granit_keyboard_state& state, std::uint32_t key) noexcept {
  return key != GRANIT_PHYSICAL_KEY_UNKNOWN && key < 256 &&
         (state.pressed_keys[key / 64] & (UINT64_C(1) << (key % 64))) != 0;
}

std::uint32_t button_bit(std::uint32_t button) noexcept {
  switch (button) {
  case UINT32_C(0x110):
    return GRANIT_POINTER_PRIMARY_BIT;
  case UINT32_C(0x111):
    return GRANIT_POINTER_SECONDARY_BIT;
  case UINT32_C(0x112):
    return GRANIT_POINTER_MIDDLE_BIT;
  case UINT32_C(0x113):
    return GRANIT_POINTER_X1_BIT;
  case UINT32_C(0x114):
    return GRANIT_POINTER_X2_BIT;
  default:
    return 0;
  }
}

} // namespace

struct wayland_input_adapter::implementation {
  xkb_context* context{xkb_context_new(XKB_CONTEXT_NO_FLAGS)};
  xkb_keymap* keymap{};
  xkb_state* state{};
};

wayland_input_adapter::wayland_input_adapter()
    : implementation_(new (std::nothrow) implementation) {}

wayland_input_adapter::~wayland_input_adapter() {
  if (implementation_ != nullptr) {
    if (implementation_->state != nullptr)
      xkb_state_unref(implementation_->state);
    if (implementation_->keymap != nullptr)
      xkb_keymap_unref(implementation_->keymap);
    if (implementation_->context != nullptr)
      xkb_context_unref(implementation_->context);
    delete implementation_;
  }
}

bool wayland_input_adapter::set_keymap(const char* text, std::size_t length) noexcept {
  if (implementation_ == nullptr || implementation_->context == nullptr || text == nullptr ||
      length == 0)
    return false;
  std::string terminated{text, length};
  auto* keymap = xkb_keymap_new_from_string(implementation_->context, terminated.c_str(),
                                            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (keymap == nullptr)
    return false;
  auto* state = xkb_state_new(keymap);
  if (state == nullptr) {
    xkb_keymap_unref(keymap);
    return false;
  }
  if (implementation_->state != nullptr)
    xkb_state_unref(implementation_->state);
  if (implementation_->keymap != nullptr)
    xkb_keymap_unref(implementation_->keymap);
  implementation_->keymap = keymap;
  implementation_->state = state;
  return true;
}

void wayland_input_adapter::modifiers(granit_window window, std::uint32_t depressed,
                                      std::uint32_t latched, std::uint32_t locked,
                                      std::uint32_t group,
                                      const wayland_input_sink& sink) noexcept {
  if (implementation_ != nullptr && implementation_->state != nullptr) {
    static_cast<void>(
        xkb_state_update_mask(implementation_->state, depressed, latched, locked, 0, 0, group));
    auto& keyboard = sink.keyboard(sink.user_data, window);
    keyboard.modifiers = 0;
    const auto add_pressed = [&](std::uint32_t physical, std::uint32_t modifier) {
      if (key_is_pressed(keyboard, physical))
        keyboard.modifiers |= modifier;
    };
    add_pressed(GRANIT_PHYSICAL_KEY_LEFT_SHIFT, GRANIT_MODIFIER_LEFT_SHIFT_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_SHIFT, GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_LEFT_CONTROL, GRANIT_MODIFIER_LEFT_CONTROL_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_CONTROL, GRANIT_MODIFIER_RIGHT_CONTROL_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_LEFT_ALT, GRANIT_MODIFIER_LEFT_ALT_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_ALT, GRANIT_MODIFIER_RIGHT_ALT_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_LEFT_SUPER, GRANIT_MODIFIER_LEFT_SUPER_BIT);
    add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_SUPER, GRANIT_MODIFIER_RIGHT_SUPER_BIT);
    const auto active = [&](const char* name) {
      return xkb_state_mod_name_is_active(implementation_->state, name, XKB_STATE_MODS_EFFECTIVE) >
             0;
    };
    if (active(XKB_MOD_NAME_CAPS))
      keyboard.modifiers |= GRANIT_MODIFIER_CAPS_LOCK_BIT;
    if (active(XKB_MOD_NAME_NUM))
      keyboard.modifiers |= GRANIT_MODIFIER_NUM_LOCK_BIT;
  }
}

void wayland_input_adapter::key(granit_window window, std::uint32_t key, bool pressed,
                                const wayland_input_sink& sink) {
  auto& keyboard = sink.keyboard(sink.user_data, window);
  const auto physical = physical_key(key);
  const auto xkb_key = static_cast<xkb_keycode_t>(key + 8);
  const auto symbol = implementation_ != nullptr && implementation_->state != nullptr
                          ? xkb_state_key_get_one_sym(implementation_->state, xkb_key)
                          : XKB_KEY_NoSymbol;
  const bool repeated = pressed && key_is_pressed(keyboard, physical);
  set_key(keyboard, physical, pressed);
  keyboard.modifiers = 0;
  const auto add_pressed = [&](std::uint32_t physical_key, std::uint32_t modifier) {
    if (key_is_pressed(keyboard, physical_key))
      keyboard.modifiers |= modifier;
  };
  add_pressed(GRANIT_PHYSICAL_KEY_LEFT_SHIFT, GRANIT_MODIFIER_LEFT_SHIFT_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_SHIFT, GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_LEFT_CONTROL, GRANIT_MODIFIER_LEFT_CONTROL_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_CONTROL, GRANIT_MODIFIER_RIGHT_CONTROL_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_LEFT_ALT, GRANIT_MODIFIER_LEFT_ALT_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_ALT, GRANIT_MODIFIER_RIGHT_ALT_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_LEFT_SUPER, GRANIT_MODIFIER_LEFT_SUPER_BIT);
  add_pressed(GRANIT_PHYSICAL_KEY_RIGHT_SUPER, GRANIT_MODIFIER_RIGHT_SUPER_BIT);
  if (implementation_ != nullptr && implementation_->state != nullptr) {
    const auto active = [&](const char* name) {
      return xkb_state_mod_name_is_active(implementation_->state, name, XKB_STATE_MODS_EFFECTIVE) >
             0;
    };
    if (active(XKB_MOD_NAME_CAPS))
      keyboard.modifiers |= GRANIT_MODIFIER_CAPS_LOCK_BIT;
    if (active(XKB_MOD_NAME_NUM))
      keyboard.modifiers |= GRANIT_MODIFIER_NUM_LOCK_BIT;
  }
  granit_input_event_data data{};
  data.key.physical_key = physical;
  data.key.logical_key = logical_key(symbol);
  data.key.modifiers = keyboard.modifiers;
  data.key.action = pressed ? (repeated ? GRANIT_KEY_ACTION_REPEATED : GRANIT_KEY_ACTION_PRESSED)
                            : GRANIT_KEY_ACTION_RELEASED;
  sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_KEY, data);
  if (pressed && implementation_ != nullptr && implementation_->state != nullptr) {
    const auto length = xkb_state_key_get_utf8(implementation_->state, xkb_key, nullptr, 0);
    if (length > 0) {
      std::string text(static_cast<std::size_t>(length), '\0');
      xkb_state_key_get_utf8(implementation_->state, xkb_key, text.data(), text.size() + 1);
      sink.text(sink.user_data, window, text);
    }
  }
}

void wayland_input_adapter::pointer_enter(granit_window window, float x, float y,
                                          const wayland_input_sink& sink) {
  auto& pointer = sink.pointer(sink.user_data, window);
  pointer.x = x;
  pointer.y = y;
  pointer.inside = 1;
  sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_ENTERED, {});
}

void wayland_input_adapter::pointer_leave(granit_window window, const wayland_input_sink& sink) {
  sink.pointer(sink.user_data, window).inside = 0;
  sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_LEFT, {});
}

void wayland_input_adapter::pointer_motion(granit_window window, float x, float y,
                                           const wayland_input_sink& sink) {
  auto& pointer = sink.pointer(sink.user_data, window);
  granit_input_event_data data{};
  data.pointer_moved.x = x;
  data.pointer_moved.y = y;
  data.pointer_moved.delta_x = x - pointer.x;
  data.pointer_moved.delta_y = y - pointer.y;
  data.pointer_moved.buttons = pointer.buttons;
  pointer.x = x;
  pointer.y = y;
  pointer.inside = 1;
  sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_MOVED, data);
}

void wayland_input_adapter::pointer_button(granit_window window, std::uint32_t button, bool pressed,
                                           const wayland_input_sink& sink) {
  auto& pointer = sink.pointer(sink.user_data, window);
  const auto bit = button_bit(button);
  if (bit == 0)
    return;
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

void wayland_input_adapter::pointer_axis(granit_window window, std::uint32_t axis, float value,
                                         const wayland_input_sink& sink) {
  auto& pointer = sink.pointer(sink.user_data, window);
  granit_input_event_data data{};
  data.pointer_wheel.x = pointer.x;
  data.pointer_wheel.y = pointer.y;
  data.pointer_wheel.delta_x = axis == 1 ? -value / 10.0F : 0.0F;
  data.pointer_wheel.delta_y = axis == 0 ? -value / 10.0F : 0.0F;
  data.pointer_wheel.buttons = pointer.buttons;
  sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_WHEEL, data);
}

void wayland_input_adapter::clear_window(granit_window) noexcept {}

} // namespace granit::input::detail
