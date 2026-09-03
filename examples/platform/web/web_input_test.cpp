// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "web_input.h"

#include <catch2/catch_all.hpp>

namespace web = granit::example::model_viewer::web;

TEST_CASE("浏览器输入按帧累积指针、滚轮和捕获状态", "[example][model-viewer][web]") {
  web::web_input adapter;
  adapter.begin_frame();
  adapter.pointer_button_changed(web::pointer_button::secondary, true);
  adapter.pointer_motion(10.0F, -3.0F);
  adapter.pointer_motion(2.0F, 1.0F);
  adapter.wheel(-4.0F);
  auto input = adapter.finish(true, false);
  CHECK(input.orbiting);
  CHECK(input.pointer_delta_x == 12.0F);
  CHECK(input.pointer_delta_y == -2.0F);
  CHECK(input.wheel_delta == -4.0F);
  CHECK(input.mouse_captured);

  adapter.begin_frame();
  input = adapter.finish(false, false);
  CHECK(input.orbiting);
  CHECK(input.pointer_delta_x == 0.0F);
  CHECK(input.wheel_delta == 0.0F);
}

TEST_CASE("浏览器输入处理快捷键和拖动终止", "[example][model-viewer][web]") {
  web::web_input adapter;
  adapter.key_pressed(web::shortcut_key::focus, false);
  adapter.key_pressed(web::shortcut_key::home, false);
  adapter.key_pressed(web::shortcut_key::focus, true);
  auto input = adapter.finish(false, true);
  CHECK(input.focus_requested);
  CHECK(input.home_requested);
  CHECK(input.keyboard_captured);

  adapter.pointer_button_changed(web::pointer_button::middle, true);
  adapter.focus_changed(false);
  input = adapter.finish(false, false);
  CHECK_FALSE(input.window_focused);
  CHECK_FALSE(input.panning);

  adapter.focus_changed(true);
  adapter.pointer_button_changed(web::pointer_button::secondary, true);
  adapter.pointer_presence_changed(false);
  input = adapter.finish(false, false);
  CHECK(input.window_focused);
  CHECK_FALSE(input.pointer_inside);
  CHECK_FALSE(input.orbiting);
}
