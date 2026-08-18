// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "input/wayland_input_adapter.h"

#include <xkbcommon/xkbcommon.h>

#include <catch2/catch_all.hpp>

#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct sink_state {
  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  granit_pointer_state pointer = GRANIT_POINTER_STATE_INIT;
  std::vector<granit_input_event> events;
  std::string text;
};

granit_keyboard_state& keyboard(void* data, granit_window) {
  return static_cast<sink_state*>(data)->keyboard;
}

granit_pointer_state& pointer(void* data, granit_window) {
  return static_cast<sink_state*>(data)->pointer;
}

void event(void* data, granit_window window, std::uint32_t type,
           const granit_input_event_data& value) {
  granit_input_event result = GRANIT_INPUT_EVENT_INIT;
  result.window = window;
  result.type = type;
  result.data = value;
  static_cast<sink_state*>(data)->events.push_back(result);
}

void text(void* data, granit_window, std::string_view value) {
  static_cast<sink_state*>(data)->text.append(value);
}

std::string default_keymap() {
  xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  REQUIRE(context != nullptr);
  const xkb_rule_names names{};
  xkb_keymap* keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  REQUIRE(keymap != nullptr);
  char* serialized = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
  REQUIRE(serialized != nullptr);
  std::string result{serialized};
  std::free(serialized);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  return result;
}

} // namespace

TEST_CASE("Wayland Input 通过 XKB keymap 生成键盘与文本事件", "[input][wayland]") {
  granit::input::detail::wayland_input_adapter adapter;
  const auto keymap = default_keymap();
  REQUIRE(adapter.set_keymap(keymap.data(), keymap.size()));

  sink_state state;
  const granit::input::detail::wayland_input_sink sink{&state, keyboard, pointer, event, text};
  adapter.key(7, 30, true, sink);
  adapter.key(7, 30, false, sink);

  REQUIRE(state.events.size() == 2);
  CHECK(state.events[0].type == GRANIT_INPUT_EVENT_KEY);
  CHECK(state.events[0].data.key.physical_key == GRANIT_PHYSICAL_KEY_A);
  CHECK(state.events[0].data.key.action == GRANIT_KEY_ACTION_PRESSED);
  CHECK(state.events[1].data.key.action == GRANIT_KEY_ACTION_RELEASED);
  CHECK(state.text == "a");
}

TEST_CASE("Wayland Input 转换指针移动、按钮和滚轮", "[input][wayland]") {
  granit::input::detail::wayland_input_adapter adapter;
  sink_state state;
  const granit::input::detail::wayland_input_sink sink{&state, keyboard, pointer, event, text};

  adapter.pointer_enter(9, 10.0F, 20.0F, sink);
  adapter.pointer_motion(9, 13.0F, 25.0F, sink);
  adapter.pointer_button(9, UINT32_C(0x110), true, sink);
  adapter.pointer_axis(9, 0, -10.0F, sink);
  adapter.pointer_leave(9, sink);

  REQUIRE(state.events.size() == 5);
  CHECK(state.events[1].data.pointer_moved.delta_x == 3.0F);
  CHECK(state.events[1].data.pointer_moved.delta_y == 5.0F);
  CHECK(state.events[2].data.pointer_button.button == GRANIT_POINTER_PRIMARY_BIT);
  CHECK(state.events[3].data.pointer_wheel.delta_y == 1.0F);
  CHECK(state.pointer.inside == 0);
}
