// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "window/platform/sdl3/input.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace granit::input::detail {
namespace {

void set_key(granit_keyboard_state& state, std::uint32_t key, bool pressed) noexcept {
  if (key == GRANIT_PHYSICAL_KEY_UNKNOWN || key >= 256)
    return;
  const auto mask = UINT64_C(1) << (key % 64);
  if (pressed)
    state.pressed_keys[key / 64] |= mask;
  else
    state.pressed_keys[key / 64] &= ~mask;
}

float fixed(std::intptr_t value) noexcept {
  return static_cast<float>(static_cast<std::int32_t>(value)) / 256.0F;
}

float fixed(std::uint32_t value) noexcept {
  return static_cast<float>(static_cast<std::int32_t>(value)) / 256.0F;
}

} // namespace

void handle_sdl3_input(granit_window window, const granit_window_input_native_event& event,
                       const platform_input_sink& sink) {
  switch (event.type) {
  case GRANIT_WINDOW_INPUT_SDL3_KEY_DOWN:
  case GRANIT_WINDOW_INPUT_SDL3_KEY_UP: {
    const bool pressed = event.type == GRANIT_WINDOW_INPUT_SDL3_KEY_DOWN;
    const auto physical = event.detail < 256 ? event.detail : GRANIT_PHYSICAL_KEY_UNKNOWN;
    auto& state = sink.keyboard(sink.user_data, window);
    set_key(state, physical, pressed);
    state.modifiers = event.state;
    granit_input_event_data data{};
    data.key.physical_key = physical;
    data.key.logical_key = event.data1;
    data.key.modifiers = state.modifiers;
    data.key.action =
        pressed ? (event.data0 != 0 ? GRANIT_KEY_ACTION_REPEATED : GRANIT_KEY_ACTION_PRESSED)
                : GRANIT_KEY_ACTION_RELEASED;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_KEY, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_SDL3_TEXT:
    if (event.word != 0 && event.value > 0) {
      sink.text(sink.user_data, window,
                {reinterpret_cast<const char*>(event.word), static_cast<std::size_t>(event.value)});
    }
    return;
  case GRANIT_WINDOW_INPUT_SDL3_POINTER_MOTION: {
    auto& state = sink.pointer(sink.user_data, window);
    granit_input_event_data data{};
    data.pointer_moved.x = static_cast<float>(event.x) / 256.0F;
    data.pointer_moved.y = static_cast<float>(event.y) / 256.0F;
    data.pointer_moved.delta_x = fixed(event.value);
    data.pointer_moved.delta_y = fixed(event.data0);
    data.pointer_moved.buttons = event.state;
    state.x = data.pointer_moved.x;
    state.y = data.pointer_moved.y;
    state.buttons = event.state;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_MOVED, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_SDL3_POINTER_DOWN:
  case GRANIT_WINDOW_INPUT_SDL3_POINTER_UP: {
    const bool pressed = event.type == GRANIT_WINDOW_INPUT_SDL3_POINTER_DOWN;
    auto& state = sink.pointer(sink.user_data, window);
    state.x = static_cast<float>(event.x) / 256.0F;
    state.y = static_cast<float>(event.y) / 256.0F;
    state.buttons = event.state;
    granit_input_event_data data{};
    data.pointer_button.x = state.x;
    data.pointer_button.y = state.y;
    data.pointer_button.button = event.detail;
    data.pointer_button.pressed = pressed ? UINT32_C(1) : UINT32_C(0);
    data.pointer_button.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_BUTTON, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_SDL3_POINTER_WHEEL: {
    auto& state = sink.pointer(sink.user_data, window);
    state.x = static_cast<float>(event.x) / 256.0F;
    state.y = static_cast<float>(event.y) / 256.0F;
    granit_input_event_data data{};
    data.pointer_wheel.x = state.x;
    data.pointer_wheel.y = state.y;
    data.pointer_wheel.delta_x = fixed(event.value);
    data.pointer_wheel.delta_y = fixed(event.data0);
    data.pointer_wheel.buttons = state.buttons;
    sink.event(sink.user_data, window, GRANIT_INPUT_EVENT_POINTER_WHEEL, data);
    return;
  }
  case GRANIT_WINDOW_INPUT_SDL3_POINTER_ENTER:
  case GRANIT_WINDOW_INPUT_SDL3_POINTER_LEAVE: {
    auto& state = sink.pointer(sink.user_data, window);
    state.inside = event.type == GRANIT_WINDOW_INPUT_SDL3_POINTER_ENTER ? UINT32_C(1) : UINT32_C(0);
    sink.event(sink.user_data, window,
               state.inside != 0 ? GRANIT_INPUT_EVENT_POINTER_ENTERED
                                 : GRANIT_INPUT_EVENT_POINTER_LEFT,
               {});
    return;
  }
  default:
    return;
  }
}

} // namespace granit::input::detail
