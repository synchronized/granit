// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "sdl3_input.h"

#include <catch2/catch_all.hpp>

TEST_CASE("SDL3 输入累积鼠标操作并在帧边界清空增量", "[example][model-viewer][sdl3]") {
  using granit::example::model_viewer::desktop::sdl3_input;
  sdl3_input adapter;
  adapter.begin_frame();
  SDL_Event event{};
  event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  event.button.button = SDL_BUTTON_RIGHT;
  adapter.process(event);
  event = {};
  event.type = SDL_EVENT_MOUSE_MOTION;
  event.motion.xrel = 12.0F;
  event.motion.yrel = -4.0F;
  adapter.process(event);
  event = {};
  event.type = SDL_EVENT_MOUSE_WHEEL;
  event.wheel.y = 2.0F;
  adapter.process(event);
  auto input = adapter.finish(false, false);
  CHECK(input.orbiting);
  CHECK(input.pointer_delta_x == 12.0F);
  CHECK(input.pointer_delta_y == -4.0F);
  CHECK(input.wheel_delta == 2.0F);

  adapter.begin_frame();
  input = adapter.finish(true, false);
  CHECK(input.orbiting);
  CHECK(input.pointer_delta_x == 0.0F);
  CHECK(input.wheel_delta == 0.0F);
  CHECK(input.mouse_captured);
}

TEST_CASE("SDL3 输入处理快捷键、滚轮方向与焦点丢失", "[example][model-viewer][sdl3]") {
  using granit::example::model_viewer::desktop::sdl3_input;
  sdl3_input adapter;
  adapter.begin_frame();
  SDL_Event event{};
  event.type = SDL_EVENT_KEY_DOWN;
  event.key.key = SDLK_F;
  adapter.process(event);
  event.key.key = SDLK_HOME;
  adapter.process(event);
  event = {};
  event.type = SDL_EVENT_MOUSE_WHEEL;
  event.wheel.y = 3.0F;
  event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
  adapter.process(event);
  auto input = adapter.finish(false, true);
  CHECK(input.focus_requested);
  CHECK(input.home_requested);
  CHECK(input.wheel_delta == -3.0F);
  CHECK(input.keyboard_captured);

  event = {};
  event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  event.button.button = SDL_BUTTON_MIDDLE;
  adapter.process(event);
  event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
  adapter.process(event);
  input = adapter.finish(false, false);
  CHECK_FALSE(input.window_focused);
  CHECK_FALSE(input.panning);
}

TEST_CASE("SDL3 输入在指针离开窗口时终止拖动", "[example][model-viewer][sdl3]") {
  using granit::example::model_viewer::desktop::sdl3_input;
  sdl3_input adapter;
  SDL_Event event{};
  event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  event.button.button = SDL_BUTTON_RIGHT;
  adapter.process(event);
  event.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
  adapter.process(event);
  const auto input = adapter.finish(false, false);
  CHECK_FALSE(input.pointer_inside);
  CHECK_FALSE(input.orbiting);
}
