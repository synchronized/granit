// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "win32_input_adapter.h"

#include <windows.h>
#include <windowsx.h>

#include <cstddef>

namespace granit::input::detail {
namespace {

std::uint32_t physical_key(std::uint32_t scan_code, bool extended) noexcept {
  switch (scan_code) {
  case 0x01:
    return GRANIT_PHYSICAL_KEY_ESCAPE;
  case 0x02:
  case 0x03:
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x07:
  case 0x08:
  case 0x09:
  case 0x0a:
    return GRANIT_PHYSICAL_KEY_1 + scan_code - 0x02;
  case 0x0b:
    return GRANIT_PHYSICAL_KEY_0;
  case 0x0e:
    return GRANIT_PHYSICAL_KEY_BACKSPACE;
  case 0x0f:
    return GRANIT_PHYSICAL_KEY_TAB;
  case 0x10:
    return GRANIT_PHYSICAL_KEY_Q;
  case 0x11:
    return GRANIT_PHYSICAL_KEY_W;
  case 0x12:
    return GRANIT_PHYSICAL_KEY_E;
  case 0x13:
    return GRANIT_PHYSICAL_KEY_R;
  case 0x14:
    return GRANIT_PHYSICAL_KEY_T;
  case 0x15:
    return GRANIT_PHYSICAL_KEY_Y;
  case 0x16:
    return GRANIT_PHYSICAL_KEY_U;
  case 0x17:
    return GRANIT_PHYSICAL_KEY_I;
  case 0x18:
    return GRANIT_PHYSICAL_KEY_O;
  case 0x19:
    return GRANIT_PHYSICAL_KEY_P;
  case 0x1c:
    return GRANIT_PHYSICAL_KEY_ENTER;
  case 0x1d:
    return extended ? GRANIT_PHYSICAL_KEY_RIGHT_CONTROL : GRANIT_PHYSICAL_KEY_LEFT_CONTROL;
  case 0x1e:
    return GRANIT_PHYSICAL_KEY_A;
  case 0x1f:
    return GRANIT_PHYSICAL_KEY_S;
  case 0x20:
    return GRANIT_PHYSICAL_KEY_D;
  case 0x21:
    return GRANIT_PHYSICAL_KEY_F;
  case 0x22:
    return GRANIT_PHYSICAL_KEY_G;
  case 0x23:
    return GRANIT_PHYSICAL_KEY_H;
  case 0x24:
    return GRANIT_PHYSICAL_KEY_J;
  case 0x25:
    return GRANIT_PHYSICAL_KEY_K;
  case 0x26:
    return GRANIT_PHYSICAL_KEY_L;
  case 0x2a:
    return GRANIT_PHYSICAL_KEY_LEFT_SHIFT;
  case 0x2c:
    return GRANIT_PHYSICAL_KEY_Z;
  case 0x2d:
    return GRANIT_PHYSICAL_KEY_X;
  case 0x2e:
    return GRANIT_PHYSICAL_KEY_C;
  case 0x2f:
    return GRANIT_PHYSICAL_KEY_V;
  case 0x30:
    return GRANIT_PHYSICAL_KEY_B;
  case 0x31:
    return GRANIT_PHYSICAL_KEY_N;
  case 0x32:
    return GRANIT_PHYSICAL_KEY_M;
  case 0x36:
    return GRANIT_PHYSICAL_KEY_RIGHT_SHIFT;
  case 0x38:
    return extended ? GRANIT_PHYSICAL_KEY_RIGHT_ALT : GRANIT_PHYSICAL_KEY_LEFT_ALT;
  case 0x39:
    return GRANIT_PHYSICAL_KEY_SPACE;
  case 0x3b:
  case 0x3c:
  case 0x3d:
  case 0x3e:
  case 0x3f:
  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
    return GRANIT_PHYSICAL_KEY_F1 + scan_code - 0x3b;
  case 0x47:
    return extended ? GRANIT_PHYSICAL_KEY_HOME : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x48:
    return extended ? GRANIT_PHYSICAL_KEY_UP : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x49:
    return extended ? GRANIT_PHYSICAL_KEY_PAGE_UP : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x4b:
    return extended ? GRANIT_PHYSICAL_KEY_LEFT : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x4d:
    return extended ? GRANIT_PHYSICAL_KEY_RIGHT : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x4f:
    return extended ? GRANIT_PHYSICAL_KEY_END : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x50:
    return extended ? GRANIT_PHYSICAL_KEY_DOWN : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x51:
    return extended ? GRANIT_PHYSICAL_KEY_PAGE_DOWN : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x52:
    return extended ? GRANIT_PHYSICAL_KEY_INSERT : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x53:
    return extended ? GRANIT_PHYSICAL_KEY_DELETE : GRANIT_PHYSICAL_KEY_UNKNOWN;
  case 0x57:
    return GRANIT_PHYSICAL_KEY_F11;
  case 0x58:
    return GRANIT_PHYSICAL_KEY_F12;
  case 0x5b:
    return GRANIT_PHYSICAL_KEY_LEFT_SUPER;
  case 0x5c:
    return GRANIT_PHYSICAL_KEY_RIGHT_SUPER;
  default:
    return GRANIT_PHYSICAL_KEY_UNKNOWN;
  }
}

std::uint32_t logical_key(std::uintptr_t key) noexcept {
  switch (key) {
  case VK_RETURN:
    return GRANIT_LOGICAL_KEY_ENTER;
  case VK_ESCAPE:
    return GRANIT_LOGICAL_KEY_ESCAPE;
  case VK_BACK:
    return GRANIT_LOGICAL_KEY_BACKSPACE;
  case VK_TAB:
    return GRANIT_LOGICAL_KEY_TAB;
  case VK_SPACE:
    return GRANIT_LOGICAL_KEY_SPACE;
  case VK_LEFT:
    return GRANIT_LOGICAL_KEY_LEFT;
  case VK_RIGHT:
    return GRANIT_LOGICAL_KEY_RIGHT;
  case VK_UP:
    return GRANIT_LOGICAL_KEY_UP;
  case VK_DOWN:
    return GRANIT_LOGICAL_KEY_DOWN;
  case VK_HOME:
    return GRANIT_LOGICAL_KEY_HOME;
  case VK_END:
    return GRANIT_LOGICAL_KEY_END;
  case VK_PRIOR:
    return GRANIT_LOGICAL_KEY_PAGE_UP;
  case VK_NEXT:
    return GRANIT_LOGICAL_KEY_PAGE_DOWN;
  case VK_INSERT:
    return GRANIT_LOGICAL_KEY_INSERT;
  case VK_DELETE:
    return GRANIT_LOGICAL_KEY_DELETE;
  default:
    if (key >= VK_F1 && key <= VK_F12)
      return GRANIT_LOGICAL_KEY_F1 + static_cast<std::uint32_t>(key - VK_F1);
    return GRANIT_LOGICAL_KEY_NONE;
  }
}

bool key_pressed(const granit_keyboard_state& state, std::uint32_t key) noexcept {
  return key < 256 && (state.pressed_keys[key / 64] & (UINT64_C(1) << (key % 64))) != 0;
}

std::uint32_t modifiers(const granit_keyboard_state& state) noexcept {
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
  if ((GetKeyState(VK_CAPITAL) & 1) != 0)
    result |= GRANIT_MODIFIER_CAPS_LOCK_BIT;
  if ((GetKeyState(VK_NUMLOCK) & 1) != 0)
    result |= GRANIT_MODIFIER_NUM_LOCK_BIT;
  return result;
}

std::uint32_t pointer_buttons(std::uintptr_t word) noexcept {
  std::uint32_t result = 0;
  if ((word & MK_LBUTTON) != 0)
    result |= GRANIT_POINTER_PRIMARY_BIT;
  if ((word & MK_RBUTTON) != 0)
    result |= GRANIT_POINTER_SECONDARY_BIT;
  if ((word & MK_MBUTTON) != 0)
    result |= GRANIT_POINTER_MIDDLE_BIT;
  if ((word & MK_XBUTTON1) != 0)
    result |= GRANIT_POINTER_X1_BIT;
  if ((word & MK_XBUTTON2) != 0)
    result |= GRANIT_POINTER_X2_BIT;
  return result;
}

void emit_text(const win32_input_sink& sink, granit_window window, const wchar_t* text,
               int length) {
  char utf8[GRANIT_INPUT_TEXT_CAPACITY]{};
  const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length, utf8,
                                         static_cast<int>(sizeof(utf8)), nullptr, nullptr);
  if (count > 0)
    sink.text(sink.user_data, window, {utf8, static_cast<std::size_t>(count)});
}

} // namespace

void win32_input_adapter::handle(granit_window window, std::uint32_t message, std::uintptr_t word,
                                 std::intptr_t value, const win32_input_sink& sink) {
  if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP ||
      message == WM_SYSKEYUP) {
    const auto scan = static_cast<std::uint32_t>((value >> 16) & 0xff);
    const auto physical = physical_key(scan, (value & (INT64_C(1) << 24)) != 0);
    auto& state = sink.keyboard(sink.user_data, window);
    const bool released = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (physical != GRANIT_PHYSICAL_KEY_UNKNOWN) {
      const auto mask = UINT64_C(1) << (physical % 64);
      if (released)
        state.pressed_keys[physical / 64] &= ~mask;
      else
        state.pressed_keys[physical / 64] |= mask;
    }
    state.modifiers = modifiers(state);
    granit_input_event_data data{};
    data.key.physical_key = physical;
    data.key.logical_key = logical_key(word);
    data.key.modifiers = state.modifiers;
    data.key.action = released ? GRANIT_KEY_ACTION_RELEASED
                               : ((value & (INT64_C(1) << 30)) != 0 ? GRANIT_KEY_ACTION_REPEATED
                                                                    : GRANIT_KEY_ACTION_PRESSED);
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_KEY, data);
    return;
  }
  if (message == WM_CHAR) {
    const auto code_unit = static_cast<wchar_t>(word);
    if (code_unit >= 0xd800 && code_unit <= 0xdbff) {
      pending_high_surrogates_[window] = code_unit;
      return;
    }
    if (code_unit >= 0xdc00 && code_unit <= 0xdfff) {
      const auto found = pending_high_surrogates_.find(window);
      if (found != pending_high_surrogates_.end()) {
        const wchar_t pair[]{found->second, code_unit};
        pending_high_surrogates_.erase(found);
        emit_text(sink, window, pair, 2);
      }
      return;
    }
    pending_high_surrogates_.erase(window);
    emit_text(sink, window, &code_unit, 1);
    return;
  }
  if (message == WM_UNICHAR && word != UNICODE_NOCHAR && word <= 0x10ffff) {
    wchar_t text[2]{};
    int length = 1;
    if (word <= 0xffff) {
      text[0] = static_cast<wchar_t>(word);
    } else {
      const auto code_point = static_cast<std::uint32_t>(word - 0x10000);
      text[0] = static_cast<wchar_t>(0xd800 + (code_point >> 10));
      text[1] = static_cast<wchar_t>(0xdc00 + (code_point & 0x3ff));
      length = 2;
    }
    emit_text(sink, window, text, length);
    return;
  }

  if (message != WM_MOUSEMOVE && message != WM_MOUSELEAVE && message != WM_LBUTTONDOWN &&
      message != WM_LBUTTONUP && message != WM_RBUTTONDOWN && message != WM_RBUTTONUP &&
      message != WM_MBUTTONDOWN && message != WM_MBUTTONUP && message != WM_XBUTTONDOWN &&
      message != WM_XBUTTONUP && message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL) {
    return;
  }

  const auto x = static_cast<float>(GET_X_LPARAM(value));
  const auto y = static_cast<float>(GET_Y_LPARAM(value));
  auto& state = sink.pointer(sink.user_data, window);
  if (message == WM_MOUSEMOVE) {
    granit_input_event_data data{};
    data.pointer_moved.x = x;
    data.pointer_moved.y = y;
    data.pointer_moved.delta_x = state.inside != 0 ? x - state.x : 0.0F;
    data.pointer_moved.delta_y = state.inside != 0 ? y - state.y : 0.0F;
    data.pointer_moved.buttons = pointer_buttons(word);
    if (state.inside == 0)
      sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_ENTERED, data);
    state.x = x;
    state.y = y;
    state.buttons = data.pointer_moved.buttons;
    state.inside = 1;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_MOVED, data);
    return;
  }
  if (message == WM_MOUSELEAVE) {
    state.inside = 0;
    granit_input_event_data data{};
    data.pointer_moved.x = state.x;
    data.pointer_moved.y = state.y;
    data.pointer_moved.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_LEFT, data);
    return;
  }
  std::uint32_t button = 0;
  if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)
    button = GRANIT_POINTER_PRIMARY_BIT;
  else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
    button = GRANIT_POINTER_SECONDARY_BIT;
  else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
    button = GRANIT_POINTER_MIDDLE_BIT;
  else if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP)
    button = GET_XBUTTON_WPARAM(word) == XBUTTON1 ? GRANIT_POINTER_X1_BIT : GRANIT_POINTER_X2_BIT;
  if (button != 0) {
    const bool pressed = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                         message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
    state.x = x;
    state.y = y;
    if (pressed)
      state.buttons |= button;
    else
      state.buttons &= ~button;
    granit_input_event_data data{};
    data.pointer_button.x = x;
    data.pointer_button.y = y;
    data.pointer_button.button = button;
    data.pointer_button.pressed = pressed ? 1U : 0U;
    data.pointer_button.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_BUTTON, data);
    return;
  }
  if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
    state.x = x;
    state.y = y;
    granit_input_event_data data{};
    data.pointer_wheel.x = x;
    data.pointer_wheel.y = y;
    const auto delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(word)) / WHEEL_DELTA;
    if (message == WM_MOUSEWHEEL)
      data.pointer_wheel.delta_y = delta;
    else
      data.pointer_wheel.delta_x = delta;
    data.pointer_wheel.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_WHEEL, data);
  }
}

void win32_input_adapter::clear_window(granit_window window) noexcept {
  pending_high_surrogates_.erase(window);
}

} // namespace granit::input::detail
