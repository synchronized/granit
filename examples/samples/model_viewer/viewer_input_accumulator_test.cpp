// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_input_accumulator.h"

#include <catch2/catch_all.hpp>

namespace {

granit::input_event pointer_button(std::uint32_t button, bool pressed) {
  granit::input_event event{};
  event.type = granit::input_event_type::pointer_button;
  event.data.pointer_button.button = button;
  event.data.pointer_button.pressed = pressed ? 1U : 0U;
  return event;
}

granit::input_event pointer_motion(float delta_x, float delta_y) {
  granit::input_event event{};
  event.type = granit::input_event_type::pointer_moved;
  event.data.pointer_moved.delta_x = delta_x;
  event.data.pointer_moved.delta_y = delta_y;
  return event;
}

} // namespace

using granit::example::model_viewer::viewer_input_accumulator;

TEST_CASE("Model Viewer 输入累积拖动、滚轮和快捷键", "[model-viewer][input]") {
  viewer_input_accumulator input;
  input.begin_frame();
  input.process(pointer_button(GRANIT_POINTER_SECONDARY_BIT, true));
  input.process(pointer_motion(12.0F, -4.0F));

  granit::input_event wheel{};
  wheel.type = granit::input_event_type::pointer_wheel;
  wheel.data.pointer_wheel.delta_y = 2.0F;
  input.process(wheel);

  granit::input_event focus{};
  focus.type = granit::input_event_type::key;
  focus.data.key.physical = granit::physical_key::f;
  focus.data.key.action = granit::key_action::pressed;
  input.process(focus);

  granit::input_event home{};
  home.type = granit::input_event_type::key;
  home.data.key.logical = granit::logical_key::home;
  home.data.key.action = granit::key_action::pressed;
  input.process(home);

  const auto frame = input.finish();
  CHECK(frame.orbiting);
  CHECK(frame.pointer_delta_x == Catch::Approx(12.0F));
  CHECK(frame.pointer_delta_y == Catch::Approx(-4.0F));
  CHECK(frame.wheel_delta == Catch::Approx(2.0F));
  CHECK(frame.focus_requested);
  CHECK(frame.home_requested);
}

TEST_CASE("Model Viewer 输入按按下时的 UI 捕获状态确定拖动所有权", "[model-viewer][input]") {
  viewer_input_accumulator input;
  input.process(pointer_button(GRANIT_POINTER_SECONDARY_BIT, true), true);
  input.process(pointer_motion(10.0F, 0.0F));
  auto frame = input.finish();
  CHECK_FALSE(frame.orbiting);
  CHECK(frame.pointer_delta_x == 0.0F);

  input.process(pointer_button(GRANIT_POINTER_SECONDARY_BIT, true));
  input.process(pointer_motion(11.0F, 0.0F));
  frame = input.finish(true, false);
  CHECK(frame.orbiting);
  CHECK_FALSE(frame.mouse_captured);
  CHECK(frame.pointer_delta_x == Catch::Approx(11.0F));
}

TEST_CASE("Model Viewer 输入遵守 UI 捕获", "[model-viewer][input]") {
  viewer_input_accumulator input;
  granit::input_event wheel{};
  wheel.type = granit::input_event_type::pointer_wheel;
  wheel.data.pointer_wheel.delta_y = 3.0F;
  input.process(wheel, true, false);

  granit::input_event home{};
  home.type = granit::input_event_type::key;
  home.data.key.logical = granit::logical_key::home;
  home.data.key.action = granit::key_action::pressed;
  input.process(home, false, true);

  const auto frame = input.finish();
  CHECK(frame.wheel_delta == 0.0F);
  CHECK_FALSE(frame.home_requested);
}

TEST_CASE("Model Viewer 输入跨过背压帧并在失焦时清理", "[model-viewer][input]") {
  viewer_input_accumulator input;
  input.begin_frame();
  input.process(pointer_button(GRANIT_POINTER_MIDDLE_BIT, true));
  input.process(pointer_motion(7.0F, -3.0F));
  input.begin_frame();
  input.begin_frame();
  auto frame = input.finish();
  CHECK(frame.panning);
  CHECK(frame.pointer_delta_x == Catch::Approx(7.0F));
  CHECK(input.merged_input_frames() == 2);

  granit::window_event focus{};
  focus.type = granit::window_event_type::focus_changed;
  focus.data.focus.focused = false;
  input.process(focus);
  frame = input.finish();
  CHECK_FALSE(frame.window_focused);
  CHECK_FALSE(frame.panning);
  CHECK(frame.pointer_delta_x == 0.0F);
}
