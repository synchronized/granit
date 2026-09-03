// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "sdl3_input.h"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

namespace granit::example::model_viewer::desktop {

void sdl3_input::begin_frame() noexcept {
  input_.pointer_delta_x = 0.0F;
  input_.pointer_delta_y = 0.0F;
  input_.wheel_delta = 0.0F;
  input_.focus_requested = false;
  input_.home_requested = false;
}

void sdl3_input::process(const SDL_Event& event) noexcept {
  switch (event.type) {
  case SDL_EVENT_MOUSE_MOTION:
    input_.pointer_delta_x += event.motion.xrel;
    input_.pointer_delta_y += event.motion.yrel;
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    input_.wheel_delta +=
        event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    const auto down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    if (event.button.button == SDL_BUTTON_RIGHT)
      input_.orbiting = down;
    else if (event.button.button == SDL_BUTTON_MIDDLE)
      input_.panning = down;
    break;
  }
  case SDL_EVENT_KEY_DOWN:
    if (!event.key.repeat && event.key.key == SDLK_F)
      input_.focus_requested = true;
    else if (!event.key.repeat && event.key.key == SDLK_HOME)
      input_.home_requested = true;
    break;
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
    input_.window_focused = true;
    break;
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    input_.window_focused = false;
    input_.orbiting = false;
    input_.panning = false;
    break;
  case SDL_EVENT_WINDOW_MOUSE_ENTER:
    input_.pointer_inside = true;
    break;
  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    input_.pointer_inside = false;
    input_.orbiting = false;
    input_.panning = false;
    break;
  default:
    break;
  }
}

viewer_input sdl3_input::finish(bool mouse_captured, bool keyboard_captured) const noexcept {
  auto result = input_;
  result.mouse_captured = mouse_captured;
  result.keyboard_captured = keyboard_captured;
  return result;
}

} // namespace granit::example::model_viewer::desktop
