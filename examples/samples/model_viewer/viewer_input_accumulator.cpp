// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_input_accumulator.h"

#include <algorithm>

namespace granit::example::model_viewer {
namespace {

constexpr float maximum_pointer_delta = 32768.0F;
constexpr float maximum_wheel_delta = 128.0F;

float add_clamped(float value, float delta, float limit) noexcept {
  return std::clamp(value + delta, -limit, limit);
}

} // namespace

void viewer_input_accumulator::begin_frame() noexcept { ++begin_frame_count_; }

void viewer_input_accumulator::clear_transient() noexcept {
  input_.pointer_delta_x = 0.0F;
  input_.pointer_delta_y = 0.0F;
  input_.wheel_delta = 0.0F;
  input_.focus_requested = false;
  input_.home_requested = false;
  orbit_motion_pending_ = false;
  pan_motion_pending_ = false;
}

void viewer_input_accumulator::clear_interaction() noexcept {
  input_.orbiting = false;
  input_.panning = false;
  clear_transient();
}

void viewer_input_accumulator::process(const granit::window_event& event) noexcept {
  if (event.type != granit::window_event_type::focus_changed)
    return;
  input_.window_focused = event.data.focus.focused;
  if (!input_.window_focused)
    clear_interaction();
}

void viewer_input_accumulator::process(const granit::input_event& event, bool pointer_captured,
                                       bool keyboard_captured) noexcept {
  switch (event.type) {
  case granit::input_event_type::key:
    if (event.data.key.action == granit::key_action::released ||
        event.data.key.action == granit::key_action::repeated || keyboard_captured) {
      break;
    }
    if (event.data.key.physical == granit::physical_key::f)
      input_.focus_requested = true;
    else if (event.data.key.logical == granit::logical_key::home)
      input_.home_requested = true;
    break;
  case granit::input_event_type::pointer_entered:
    input_.pointer_inside = true;
    break;
  case granit::input_event_type::pointer_left:
    input_.pointer_inside = false;
    clear_interaction();
    break;
  case granit::input_event_type::pointer_moved:
    if (!input_.orbiting && !input_.panning)
      break;
    input_.pointer_delta_x = add_clamped(input_.pointer_delta_x, event.data.pointer_moved.delta_x,
                                         maximum_pointer_delta);
    input_.pointer_delta_y = add_clamped(input_.pointer_delta_y, event.data.pointer_moved.delta_y,
                                         maximum_pointer_delta);
    orbit_motion_pending_ = orbit_motion_pending_ || input_.orbiting;
    pan_motion_pending_ = pan_motion_pending_ || input_.panning;
    break;
  case granit::input_event_type::pointer_button: {
    const bool active = event.data.pointer_button.pressed != 0 && !pointer_captured;
    if (event.data.pointer_button.button == GRANIT_POINTER_SECONDARY_BIT)
      input_.orbiting = active;
    else if (event.data.pointer_button.button == GRANIT_POINTER_MIDDLE_BIT)
      input_.panning = active;
    break;
  }
  case granit::input_event_type::pointer_wheel:
    if (!pointer_captured) {
      input_.wheel_delta =
          add_clamped(input_.wheel_delta, event.data.pointer_wheel.delta_y, maximum_wheel_delta);
    }
    break;
  case granit::input_event_type::none:
  case granit::input_event_type::text:
    break;
  }
}

viewer_input viewer_input_accumulator::finish(bool pointer_captured,
                                              bool keyboard_captured) noexcept {
  auto result = input_;
  result.orbiting = result.orbiting || orbit_motion_pending_;
  result.panning = result.panning || pan_motion_pending_;
  result.mouse_captured = pointer_captured && !result.orbiting && !result.panning;
  result.keyboard_captured = keyboard_captured;
  if (begin_frame_count_ > 0)
    merged_input_frames_ += begin_frame_count_ - 1;
  begin_frame_count_ = 0;
  clear_transient();
  return result;
}

} // namespace granit::example::model_viewer
