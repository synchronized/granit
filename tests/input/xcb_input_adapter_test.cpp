// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/xcb/input_adapter.h"

#include <catch2/catch_all.hpp>

#include <vector>

namespace {

struct capture {
  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  granit_pointer_state pointer = GRANIT_POINTER_STATE_INIT;
  std::vector<granit_input_event> events;
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
  output.type = type;
  output.window = window;
  output.data = data;
  static_cast<capture*>(user_data)->events.push_back(output);
}

granit::input::detail::xcb_input_sink sink(capture& output) {
  return {&output, keyboard, pointer, event};
}

} // namespace

TEST_CASE("XCB Input 转换物理键、逻辑键与重复状态", "[input][xcb]") {
  capture output;
  const auto input_sink = sink(output);

  granit::input::detail::handle_xcb_input(7, {2, 0, 0, 0, 38}, input_sink);
  granit::input::detail::handle_xcb_input(7, {2, 0, 0, 0, 38}, input_sink);
  granit::input::detail::handle_xcb_input(7, {3, 0, 0, 0, 38}, input_sink);
  granit::input::detail::handle_xcb_input(7, {2, 0, 0, 0, 113}, input_sink);

  REQUIRE(output.events.size() == 4);
  CHECK(output.events[0].data.key.physical_key == GRANIT_PHYSICAL_KEY_A);
  CHECK(output.events[0].data.key.logical_key == GRANIT_LOGICAL_KEY_NONE);
  CHECK(output.events[0].data.key.action == GRANIT_KEY_ACTION_PRESSED);
  CHECK(output.events[1].data.key.action == GRANIT_KEY_ACTION_REPEATED);
  CHECK(output.events[2].data.key.action == GRANIT_KEY_ACTION_RELEASED);
  CHECK(output.events[3].data.key.physical_key == GRANIT_PHYSICAL_KEY_LEFT);
  CHECK(output.events[3].data.key.logical_key == GRANIT_LOGICAL_KEY_LEFT);
}

TEST_CASE("XCB Input 转换指针移动、按钮、滚轮与边界", "[input][xcb]") {
  capture output;
  const auto input_sink = sink(output);

  granit::input::detail::handle_xcb_input(9, {7, 10, 12, 0, 0}, input_sink);
  granit::input::detail::handle_xcb_input(9, {6, 14, 20, 0, 0}, input_sink);
  granit::input::detail::handle_xcb_input(9, {4, 14, 20, 0, 1}, input_sink);
  granit::input::detail::handle_xcb_input(9, {4, 14, 20, 0, 4}, input_sink);
  granit::input::detail::handle_xcb_input(9, {5, 14, 20, 0, 1}, input_sink);
  granit::input::detail::handle_xcb_input(9, {8, 14, 20, 0, 0}, input_sink);

  REQUIRE(output.events.size() == 6);
  CHECK(output.events[0].type == GRANIT_INPUT_EVENT_POINTER_ENTERED);
  CHECK(output.events[1].data.pointer_moved.delta_x == 4.0F);
  CHECK(output.events[1].data.pointer_moved.delta_y == 8.0F);
  CHECK(output.events[2].data.pointer_button.button == GRANIT_POINTER_PRIMARY_BIT);
  CHECK(output.events[2].data.pointer_button.pressed == 1);
  CHECK(output.events[3].type == GRANIT_INPUT_EVENT_POINTER_WHEEL);
  CHECK(output.events[3].data.pointer_wheel.delta_y == 1.0F);
  CHECK(output.events[4].data.pointer_button.pressed == 0);
  CHECK(output.events[5].type == GRANIT_INPUT_EVENT_POINTER_LEFT);
  CHECK(output.pointer.inside == 0);
  CHECK(output.pointer.buttons == 0);
}
