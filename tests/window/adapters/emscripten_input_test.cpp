// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "window/platform/emscripten/input.h"

#include <catch2/catch_all.hpp>

#include <string>
#include <vector>

namespace {

struct capture {
  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  granit_pointer_state pointer = GRANIT_POINTER_STATE_INIT;
  std::vector<granit_input_event> events;
  std::string text;
};

granit_keyboard_state& keyboard(void* user_data, granit_window) {
  return static_cast<capture*>(user_data)->keyboard;
}

granit_pointer_state& pointer(void* user_data, granit_window) {
  return static_cast<capture*>(user_data)->pointer;
}

void event(void* user_data, granit_window window, std::uint32_t type,
           const granit_input_event_data& data) {
  granit_input_event output = GRANIT_INPUT_EVENT_INIT;
  output.window = window;
  output.type = type;
  output.data = data;
  static_cast<capture*>(user_data)->events.push_back(output);
}

void text(void* user_data, granit_window, std::string_view value) {
  static_cast<capture*>(user_data)->text.append(value);
}

granit::input::detail::emscripten_input_sink sink(capture& output) {
  return {&output, keyboard, pointer, event, text};
}

granit_window_input_native_event key_event(std::uint32_t type, const char* code, const char* key,
                                           bool repeated = false) {
  granit_window_input_native_event result{};
  result.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  result.type = type;
  result.word = reinterpret_cast<std::uintptr_t>(code);
  result.value = reinterpret_cast<std::intptr_t>(key);
  result.data0 = repeated ? UINT32_C(1) : UINT32_C(0);
  return result;
}

} // namespace

TEST_CASE("Emscripten Input 转换物理键、逻辑键、重复与文本", "[input][emscripten]") {
  granit::input::detail::emscripten_input_adapter adapter;
  capture output;
  const auto input_sink = sink(output);

  auto shift = key_event(GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN, "ShiftRight", "Shift");
  shift.state = GRANIT_MODIFIER_LEFT_SHIFT_BIT;
  adapter.handle(7, shift, input_sink);
  adapter.handle(7, key_event(GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN, "KeyA", "a"), input_sink);
  adapter.handle(7, key_event(GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN, "KeyA", "a", true),
                 input_sink);
  adapter.handle(7, key_event(GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_UP, "KeyA", "a"), input_sink);

  constexpr char committed_text[] = "\xE4\xBD\xA0";
  granit_window_input_native_event committed{};
  committed.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  committed.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_TEXT;
  committed.word = reinterpret_cast<std::uintptr_t>(committed_text);
  committed.value = static_cast<std::intptr_t>(sizeof(committed_text) - 1);
  adapter.handle(7, committed, input_sink);

  REQUIRE(output.events.size() == 4);
  CHECK(output.events[0].data.key.physical_key == GRANIT_PHYSICAL_KEY_RIGHT_SHIFT);
  CHECK(output.events[0].data.key.modifiers == GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
  CHECK(output.events[1].data.key.physical_key == GRANIT_PHYSICAL_KEY_A);
  CHECK(output.events[1].data.key.action == GRANIT_KEY_ACTION_PRESSED);
  CHECK(output.events[2].data.key.action == GRANIT_KEY_ACTION_REPEATED);
  CHECK(output.events[3].data.key.action == GRANIT_KEY_ACTION_RELEASED);
  CHECK(output.text == committed_text);
}

TEST_CASE("Emscripten Input 按物理位置转换数字键", "[input][emscripten]") {
  granit::input::detail::emscripten_input_adapter adapter;
  capture output;
  const auto input_sink = sink(output);

  adapter.handle(7, key_event(GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN, "Digit0", "0"),
                 input_sink);
  adapter.handle(7, key_event(GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN, "Digit7", "7"),
                 input_sink);

  REQUIRE(output.events.size() == 2);
  CHECK(output.events[0].data.key.physical_key == GRANIT_PHYSICAL_KEY_0);
  CHECK(output.events[1].data.key.physical_key == GRANIT_PHYSICAL_KEY_7);
}

TEST_CASE("Emscripten Input 转换指针、按钮与连续滚轮", "[input][emscripten]") {
  granit::input::detail::emscripten_input_adapter adapter;
  capture output;
  const auto input_sink = sink(output);

  granit_window_input_native_event input{};
  input.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  input.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_ENTER;
  input.x = 10;
  input.y = 20;
  adapter.handle(9, input, input_sink);
  input.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_MOVE;
  input.x = 14;
  input.y = 27;
  adapter.handle(9, input, input_sink);
  input.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_DOWN;
  input.word = 2;
  input.detail = GRANIT_POINTER_SECONDARY_BIT;
  adapter.handle(9, input, input_sink);
  input.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_WHEEL;
  input.data0 = static_cast<std::uint32_t>(INT32_C(64));
  input.data1 = static_cast<std::uint32_t>(INT32_C(-384));
  adapter.handle(9, input, input_sink);
  input.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_LEAVE;
  adapter.handle(9, input, input_sink);

  REQUIRE(output.events.size() == 5);
  CHECK(output.events[0].type == GRANIT_INPUT_EVENT_POINTER_ENTERED);
  CHECK(output.events[1].data.pointer_moved.delta_x == 4.0F);
  CHECK(output.events[1].data.pointer_moved.delta_y == 7.0F);
  CHECK(output.events[2].data.pointer_button.button == GRANIT_POINTER_SECONDARY_BIT);
  CHECK(output.events[3].data.pointer_wheel.delta_x == Catch::Approx(0.25F));
  CHECK(output.events[3].data.pointer_wheel.delta_y == Catch::Approx(-1.5F));
  CHECK(output.events[4].type == GRANIT_INPUT_EVENT_POINTER_LEFT);
  CHECK(output.pointer.inside == 0);
}
