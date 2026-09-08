// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/desktop/sdl3_input.h"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

#include <algorithm>

namespace granit::example::model_viewer::desktop {
namespace {

constexpr float maximum_pointer_delta = 32768.0F;
constexpr float maximum_wheel_delta = 128.0F;

float add_clamped(float value, float delta, float limit) noexcept {
  return std::clamp(value + delta, -limit, limit);
}

} // namespace

void sdl3_input::begin_frame() noexcept { ++begin_frame_count_; }

void sdl3_input::clear_transient() noexcept {
  input_.pointer_delta_x = 0.0F;
  input_.pointer_delta_y = 0.0F;
  input_.wheel_delta = 0.0F;
  input_.focus_requested = false;
  input_.home_requested = false;
  orbit_motion_pending_ = false;
  pan_motion_pending_ = false;
}

void sdl3_input::process(const SDL_Event& event, bool mouse_captured,
                         bool keyboard_captured) noexcept {
  switch (event.type) {
  case SDL_EVENT_MOUSE_MOTION:
    // 拖动所有权在按钮按下时确定；跨过 UI 面板不会中断从视口开始的操作。
    if (input_.orbiting || input_.panning) {
      input_.pointer_delta_x =
          add_clamped(input_.pointer_delta_x, event.motion.xrel, maximum_pointer_delta);
      input_.pointer_delta_y =
          add_clamped(input_.pointer_delta_y, event.motion.yrel, maximum_pointer_delta);
      orbit_motion_pending_ = orbit_motion_pending_ || input_.orbiting;
      pan_motion_pending_ = pan_motion_pending_ || input_.panning;
    }
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    if (!mouse_captured) {
      const auto delta =
          event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
      input_.wheel_delta = add_clamped(input_.wheel_delta, delta, maximum_wheel_delta);
    }
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    const auto down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    if (event.button.button == SDL_BUTTON_RIGHT)
      input_.orbiting = down && !mouse_captured;
    else if (event.button.button == SDL_BUTTON_MIDDLE)
      input_.panning = down && !mouse_captured;
    break;
  }
  case SDL_EVENT_KEY_DOWN:
    if (!keyboard_captured && !event.key.repeat && event.key.key == SDLK_F)
      input_.focus_requested = true;
    else if (!keyboard_captured && !event.key.repeat && event.key.key == SDLK_HOME)
      input_.home_requested = true;
    break;
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
    input_.window_focused = true;
    break;
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    input_.window_focused = false;
    input_.orbiting = false;
    input_.panning = false;
    clear_transient();
    break;
  case SDL_EVENT_WINDOW_MOUSE_ENTER:
    input_.pointer_inside = true;
    break;
  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    input_.pointer_inside = false;
    input_.orbiting = false;
    input_.panning = false;
    clear_transient();
    break;
  default:
    break;
  }
}

viewer_input sdl3_input::finish(bool mouse_captured, bool keyboard_captured) noexcept {
  auto result = input_;
  result.orbiting = result.orbiting || orbit_motion_pending_;
  result.panning = result.panning || pan_motion_pending_;
  // 已在事件入口过滤 UI 拥有的操作；这里只保留没有相机手势时的兼容状态。
  result.mouse_captured = mouse_captured && !result.orbiting && !result.panning;
  result.keyboard_captured = keyboard_captured;
  // 跨过多次帧边界的输入被合并进同一个提交帧；只统计被合并（未独立提交）的额外帧。
  if (begin_frame_count_ > 0)
    merged_input_frames_ += begin_frame_count_ - 1;
  begin_frame_count_ = 0;
  clear_transient();
  return result;
}

} // namespace granit::example::model_viewer::desktop
