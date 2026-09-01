// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "web_input.h"

namespace granit::example::model_viewer::web {

void web_input::begin_frame() noexcept {
  input_.pointer_delta_x = 0.0F;
  input_.pointer_delta_y = 0.0F;
  input_.wheel_delta = 0.0F;
  input_.focus_requested = false;
  input_.home_requested = false;
}

void web_input::pointer_motion(float delta_x, float delta_y) noexcept {
  input_.pointer_delta_x += delta_x;
  input_.pointer_delta_y += delta_y;
}

void web_input::pointer_button_changed(pointer_button button, bool pressed) noexcept {
  if (button == pointer_button::secondary)
    input_.orbiting = pressed;
  else if (button == pointer_button::middle)
    input_.panning = pressed;
}

void web_input::wheel(float delta_y) noexcept { input_.wheel_delta += delta_y; }

void web_input::key_pressed(shortcut_key key, bool repeat) noexcept {
  if (repeat)
    return;
  if (key == shortcut_key::focus)
    input_.focus_requested = true;
  else if (key == shortcut_key::home)
    input_.home_requested = true;
}

void web_input::focus_changed(bool focused) noexcept {
  input_.window_focused = focused;
  if (!focused) {
    input_.orbiting = false;
    input_.panning = false;
  }
}

void web_input::pointer_presence_changed(bool inside) noexcept {
  input_.pointer_inside = inside;
  if (!inside) {
    input_.orbiting = false;
    input_.panning = false;
  }
}

viewer_input web_input::finish(bool mouse_captured, bool keyboard_captured) const noexcept {
  auto result = input_;
  result.mouse_captured = mouse_captured;
  result.keyboard_captured = keyboard_captured;
  return result;
}

} // namespace granit::example::model_viewer::web
