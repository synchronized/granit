// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "window/platform/emscripten/input.h"

#include "window/input/state_utils.h"

#include <array>
#include <string_view>

namespace granit::input::detail {
namespace {

struct key_name {
  std::string_view name;
  std::uint32_t value;
};

constexpr std::array physical_names{
    key_name{"Enter", GRANIT_PHYSICAL_KEY_ENTER},
    key_name{"Escape", GRANIT_PHYSICAL_KEY_ESCAPE},
    key_name{"Backspace", GRANIT_PHYSICAL_KEY_BACKSPACE},
    key_name{"Tab", GRANIT_PHYSICAL_KEY_TAB},
    key_name{"Space", GRANIT_PHYSICAL_KEY_SPACE},
    key_name{"F1", GRANIT_PHYSICAL_KEY_F1},
    key_name{"F2", GRANIT_PHYSICAL_KEY_F2},
    key_name{"F3", GRANIT_PHYSICAL_KEY_F3},
    key_name{"F4", GRANIT_PHYSICAL_KEY_F4},
    key_name{"F5", GRANIT_PHYSICAL_KEY_F5},
    key_name{"F6", GRANIT_PHYSICAL_KEY_F6},
    key_name{"F7", GRANIT_PHYSICAL_KEY_F7},
    key_name{"F8", GRANIT_PHYSICAL_KEY_F8},
    key_name{"F9", GRANIT_PHYSICAL_KEY_F9},
    key_name{"F10", GRANIT_PHYSICAL_KEY_F10},
    key_name{"F11", GRANIT_PHYSICAL_KEY_F11},
    key_name{"F12", GRANIT_PHYSICAL_KEY_F12},
    key_name{"Insert", GRANIT_PHYSICAL_KEY_INSERT},
    key_name{"Home", GRANIT_PHYSICAL_KEY_HOME},
    key_name{"PageUp", GRANIT_PHYSICAL_KEY_PAGE_UP},
    key_name{"Delete", GRANIT_PHYSICAL_KEY_DELETE},
    key_name{"End", GRANIT_PHYSICAL_KEY_END},
    key_name{"PageDown", GRANIT_PHYSICAL_KEY_PAGE_DOWN},
    key_name{"ArrowRight", GRANIT_PHYSICAL_KEY_RIGHT},
    key_name{"ArrowLeft", GRANIT_PHYSICAL_KEY_LEFT},
    key_name{"ArrowDown", GRANIT_PHYSICAL_KEY_DOWN},
    key_name{"ArrowUp", GRANIT_PHYSICAL_KEY_UP},
    key_name{"ControlLeft", GRANIT_PHYSICAL_KEY_LEFT_CONTROL},
    key_name{"ShiftLeft", GRANIT_PHYSICAL_KEY_LEFT_SHIFT},
    key_name{"AltLeft", GRANIT_PHYSICAL_KEY_LEFT_ALT},
    key_name{"MetaLeft", GRANIT_PHYSICAL_KEY_LEFT_SUPER},
    key_name{"ControlRight", GRANIT_PHYSICAL_KEY_RIGHT_CONTROL},
    key_name{"ShiftRight", GRANIT_PHYSICAL_KEY_RIGHT_SHIFT},
    key_name{"AltRight", GRANIT_PHYSICAL_KEY_RIGHT_ALT},
    key_name{"MetaRight", GRANIT_PHYSICAL_KEY_RIGHT_SUPER},
};

constexpr std::array logical_names{
    key_name{"Enter", GRANIT_LOGICAL_KEY_ENTER},
    key_name{"Escape", GRANIT_LOGICAL_KEY_ESCAPE},
    key_name{"Backspace", GRANIT_LOGICAL_KEY_BACKSPACE},
    key_name{"Tab", GRANIT_LOGICAL_KEY_TAB},
    key_name{" ", GRANIT_LOGICAL_KEY_SPACE},
    key_name{"ArrowLeft", GRANIT_LOGICAL_KEY_LEFT},
    key_name{"ArrowRight", GRANIT_LOGICAL_KEY_RIGHT},
    key_name{"ArrowUp", GRANIT_LOGICAL_KEY_UP},
    key_name{"ArrowDown", GRANIT_LOGICAL_KEY_DOWN},
    key_name{"Home", GRANIT_LOGICAL_KEY_HOME},
    key_name{"End", GRANIT_LOGICAL_KEY_END},
    key_name{"PageUp", GRANIT_LOGICAL_KEY_PAGE_UP},
    key_name{"PageDown", GRANIT_LOGICAL_KEY_PAGE_DOWN},
    key_name{"Insert", GRANIT_LOGICAL_KEY_INSERT},
    key_name{"Delete", GRANIT_LOGICAL_KEY_DELETE},
    key_name{"F1", GRANIT_LOGICAL_KEY_F1},
    key_name{"F2", GRANIT_LOGICAL_KEY_F2},
    key_name{"F3", GRANIT_LOGICAL_KEY_F3},
    key_name{"F4", GRANIT_LOGICAL_KEY_F4},
    key_name{"F5", GRANIT_LOGICAL_KEY_F5},
    key_name{"F6", GRANIT_LOGICAL_KEY_F6},
    key_name{"F7", GRANIT_LOGICAL_KEY_F7},
    key_name{"F8", GRANIT_LOGICAL_KEY_F8},
    key_name{"F9", GRANIT_LOGICAL_KEY_F9},
    key_name{"F10", GRANIT_LOGICAL_KEY_F10},
    key_name{"F11", GRANIT_LOGICAL_KEY_F11},
    key_name{"F12", GRANIT_LOGICAL_KEY_F12},
};

std::uint32_t physical_from_code(std::string_view code) noexcept {
  if (code.size() == 4 && code.starts_with("Key") && code[3] >= 'A' && code[3] <= 'Z')
    return GRANIT_PHYSICAL_KEY_A + static_cast<std::uint32_t>(code[3] - 'A');
  if (code.size() == 6 && code.starts_with("Digit") && code[5] >= '0' && code[5] <= '9')
    return code[5] == '0' ? GRANIT_PHYSICAL_KEY_0
                          : GRANIT_PHYSICAL_KEY_1 + static_cast<std::uint32_t>(code[5] - '1');
  for (const auto& entry : physical_names) {
    if (entry.name == code)
      return entry.value;
  }
  return GRANIT_PHYSICAL_KEY_UNKNOWN;
}

std::uint32_t logical_from_key(std::string_view key) noexcept {
  for (const auto& entry : logical_names) {
    if (entry.name == key)
      return entry.value;
  }
  return GRANIT_LOGICAL_KEY_NONE;
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

std::uint32_t merge_dom_modifiers(const granit_keyboard_state& state,
                                  std::uint32_t aggregate) noexcept {
  auto result = pressed_key_modifiers(state);
  const auto add_fallback = [&](std::uint32_t aggregate_bit, std::uint32_t left,
                                std::uint32_t right) {
    if ((aggregate & aggregate_bit) != 0 && (result & (left | right)) == 0)
      result |= left;
  };
  add_fallback(GRANIT_MODIFIER_LEFT_SHIFT_BIT, GRANIT_MODIFIER_LEFT_SHIFT_BIT,
               GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
  add_fallback(GRANIT_MODIFIER_LEFT_CONTROL_BIT, GRANIT_MODIFIER_LEFT_CONTROL_BIT,
               GRANIT_MODIFIER_RIGHT_CONTROL_BIT);
  add_fallback(GRANIT_MODIFIER_LEFT_ALT_BIT, GRANIT_MODIFIER_LEFT_ALT_BIT,
               GRANIT_MODIFIER_RIGHT_ALT_BIT);
  add_fallback(GRANIT_MODIFIER_LEFT_SUPER_BIT, GRANIT_MODIFIER_LEFT_SUPER_BIT,
               GRANIT_MODIFIER_RIGHT_SUPER_BIT);
  return result;
}

std::uint32_t pointer_button_from_dom(std::uintptr_t button) noexcept {
  switch (button) {
  case 0:
    return GRANIT_POINTER_PRIMARY_BIT;
  case 1:
    return GRANIT_POINTER_MIDDLE_BIT;
  case 2:
    return GRANIT_POINTER_SECONDARY_BIT;
  case 3:
    return GRANIT_POINTER_X1_BIT;
  case 4:
    return GRANIT_POINTER_X2_BIT;
  default:
    return 0;
  }
}

} // namespace

void emscripten_input_adapter::handle(granit_window window,
                                      const granit_window_input_native_event& event,
                                      const emscripten_input_sink& sink) {
  switch (event.type) {
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN:
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_UP: {
    const bool pressed = event.type == GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN;
    const auto code = reinterpret_cast<const char*>(event.word);
    const auto key = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(event.value));
    const auto physical =
        physical_from_code(code != nullptr ? std::string_view{code} : std::string_view{});
    auto& state = sink.keyboard(sink.user_data, window);
    set_key(state, physical, pressed);
    state.modifiers = merge_dom_modifiers(state, event.state);
    granit_input_event_data data{};
    data.key.physical_key = physical;
    data.key.logical_key =
        logical_from_key(key != nullptr ? std::string_view{key} : std::string_view{});
    data.key.modifiers = state.modifiers;
    data.key.action =
        pressed ? (event.data0 != 0 ? GRANIT_KEY_ACTION_REPEATED : GRANIT_KEY_ACTION_PRESSED)
                : GRANIT_KEY_ACTION_RELEASED;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_KEY, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_TEXT:
    if (event.word != 0 && event.value > 0) {
      sink.text(sink.user_data, window,
                {reinterpret_cast<const char*>(event.word), static_cast<std::size_t>(event.value)});
    }
    return;
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_MOVE: {
    auto& state = sink.pointer(sink.user_data, window);
    granit_input_event_data data{};
    data.pointer_moved.x = static_cast<float>(event.x);
    data.pointer_moved.y = static_cast<float>(event.y);
    data.pointer_moved.delta_x = state.inside != 0 ? data.pointer_moved.x - state.x : 0.0F;
    data.pointer_moved.delta_y = state.inside != 0 ? data.pointer_moved.y - state.y : 0.0F;
    data.pointer_moved.buttons = event.detail;
    if (state.inside == 0)
      sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_ENTERED, {});
    state.x = data.pointer_moved.x;
    state.y = data.pointer_moved.y;
    state.buttons = event.detail;
    state.inside = 1;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_MOVED, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_DOWN:
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_UP: {
    const bool pressed = event.type == GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_DOWN;
    const auto button = pointer_button_from_dom(event.word);
    if (button == 0)
      return;
    auto& state = sink.pointer(sink.user_data, window);
    state.x = static_cast<float>(event.x);
    state.y = static_cast<float>(event.y);
    state.buttons = event.detail;
    if (pressed)
      state.buttons |= button;
    else
      state.buttons &= ~button;
    granit_input_event_data data{};
    data.pointer_button.x = state.x;
    data.pointer_button.y = state.y;
    data.pointer_button.button = button;
    data.pointer_button.pressed = pressed ? UINT32_C(1) : UINT32_C(0);
    data.pointer_button.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_BUTTON, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_WHEEL: {
    auto& state = sink.pointer(sink.user_data, window);
    state.x = static_cast<float>(event.x);
    state.y = static_cast<float>(event.y);
    state.buttons = event.detail;
    granit_input_event_data data{};
    data.pointer_wheel.x = state.x;
    data.pointer_wheel.y = state.y;
    data.pointer_wheel.delta_x =
        static_cast<float>(static_cast<std::int32_t>(event.data0)) / 256.0F;
    data.pointer_wheel.delta_y =
        static_cast<float>(static_cast<std::int32_t>(event.data1)) / 256.0F;
    data.pointer_wheel.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_WHEEL, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_ENTER: {
    auto& state = sink.pointer(sink.user_data, window);
    state.x = static_cast<float>(event.x);
    state.y = static_cast<float>(event.y);
    state.buttons = event.detail;
    state.inside = 1;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_ENTERED, {});
    return;
  }
  case GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_LEAVE: {
    auto& state = sink.pointer(sink.user_data, window);
    state.x = static_cast<float>(event.x);
    state.y = static_cast<float>(event.y);
    state.buttons = event.detail;
    state.inside = 0;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_LEFT, {});
    return;
  }
  default:
    return;
  }
}

void emscripten_input_adapter::clear_window(granit_window) noexcept {}

} // namespace granit::input::detail
