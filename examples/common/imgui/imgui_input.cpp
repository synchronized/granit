// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "imgui/imgui_input.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>

namespace granit::example::imgui {
namespace {

ImGuiKey map_key(granit::physical_key key) noexcept {
  const auto value = static_cast<std::uint32_t>(key);
  if (value >= static_cast<std::uint32_t>(granit::physical_key::a) &&
      value <= static_cast<std::uint32_t>(granit::physical_key::z)) {
    return static_cast<ImGuiKey>(ImGuiKey_A + value -
                                 static_cast<std::uint32_t>(granit::physical_key::a));
  }
  if (value >= static_cast<std::uint32_t>(granit::physical_key::digit_1) &&
      value <= static_cast<std::uint32_t>(granit::physical_key::digit_9)) {
    return static_cast<ImGuiKey>(ImGuiKey_1 + value -
                                 static_cast<std::uint32_t>(granit::physical_key::digit_1));
  }
  if (key == granit::physical_key::digit_0)
    return ImGuiKey_0;
  if (value >= static_cast<std::uint32_t>(granit::physical_key::f1) &&
      value <= static_cast<std::uint32_t>(granit::physical_key::f12)) {
    return static_cast<ImGuiKey>(ImGuiKey_F1 + value -
                                 static_cast<std::uint32_t>(granit::physical_key::f1));
  }
  switch (key) {
  case granit::physical_key::enter:
    return ImGuiKey_Enter;
  case granit::physical_key::escape:
    return ImGuiKey_Escape;
  case granit::physical_key::backspace:
    return ImGuiKey_Backspace;
  case granit::physical_key::tab:
    return ImGuiKey_Tab;
  case granit::physical_key::space:
    return ImGuiKey_Space;
  case granit::physical_key::insert:
    return ImGuiKey_Insert;
  case granit::physical_key::home:
    return ImGuiKey_Home;
  case granit::physical_key::page_up:
    return ImGuiKey_PageUp;
  case granit::physical_key::delete_key:
    return ImGuiKey_Delete;
  case granit::physical_key::end:
    return ImGuiKey_End;
  case granit::physical_key::page_down:
    return ImGuiKey_PageDown;
  case granit::physical_key::right:
    return ImGuiKey_RightArrow;
  case granit::physical_key::left:
    return ImGuiKey_LeftArrow;
  case granit::physical_key::down:
    return ImGuiKey_DownArrow;
  case granit::physical_key::up:
    return ImGuiKey_UpArrow;
  case granit::physical_key::left_control:
    return ImGuiKey_LeftCtrl;
  case granit::physical_key::left_shift:
    return ImGuiKey_LeftShift;
  case granit::physical_key::left_alt:
    return ImGuiKey_LeftAlt;
  case granit::physical_key::left_super:
    return ImGuiKey_LeftSuper;
  case granit::physical_key::right_control:
    return ImGuiKey_RightCtrl;
  case granit::physical_key::right_shift:
    return ImGuiKey_RightShift;
  case granit::physical_key::right_alt:
    return ImGuiKey_RightAlt;
  case granit::physical_key::right_super:
    return ImGuiKey_RightSuper;
  default:
    return ImGuiKey_None;
  }
}

int map_button(std::uint32_t button) noexcept {
  switch (button) {
  case GRANIT_POINTER_PRIMARY_BIT:
    return ImGuiMouseButton_Left;
  case GRANIT_POINTER_SECONDARY_BIT:
    return ImGuiMouseButton_Right;
  case GRANIT_POINTER_MIDDLE_BIT:
    return ImGuiMouseButton_Middle;
  case GRANIT_POINTER_X1_BIT:
    return 3;
  case GRANIT_POINTER_X2_BIT:
    return 4;
  default:
    return -1;
  }
}

void update_modifiers(ImGuiIO& io, std::uint32_t modifiers) noexcept {
  io.AddKeyEvent(ImGuiMod_Ctrl, (modifiers & (GRANIT_MODIFIER_LEFT_CONTROL_BIT |
                                              GRANIT_MODIFIER_RIGHT_CONTROL_BIT)) != 0);
  io.AddKeyEvent(ImGuiMod_Shift, (modifiers & (GRANIT_MODIFIER_LEFT_SHIFT_BIT |
                                               GRANIT_MODIFIER_RIGHT_SHIFT_BIT)) != 0);
  io.AddKeyEvent(ImGuiMod_Alt,
                 (modifiers & (GRANIT_MODIFIER_LEFT_ALT_BIT | GRANIT_MODIFIER_RIGHT_ALT_BIT)) != 0);
  io.AddKeyEvent(ImGuiMod_Super, (modifiers & (GRANIT_MODIFIER_LEFT_SUPER_BIT |
                                               GRANIT_MODIFIER_RIGHT_SUPER_BIT)) != 0);
}

} // namespace

void process_window_event(const granit::window_event& event) noexcept {
  if (event.type == granit::window_event_type::focus_changed)
    ImGui::GetIO().AddFocusEvent(event.data.focus.focused);
}

void process_input_event(const granit::input_event& event) noexcept {
  auto& io = ImGui::GetIO();
  switch (event.type) {
  case granit::input_event_type::key: {
    update_modifiers(io, event.data.key.modifiers);
    const auto key = map_key(event.data.key.physical);
    if (key != ImGuiKey_None)
      io.AddKeyEvent(key, event.data.key.action != granit::key_action::released);
    break;
  }
  case granit::input_event_type::text: {
    std::array<char, GRANIT_INPUT_TEXT_CAPACITY + 1> text{};
    const auto length = std::min<std::size_t>(event.data.text.length, GRANIT_INPUT_TEXT_CAPACITY);
    std::copy_n(event.data.text.utf8, length, text.data());
    io.AddInputCharactersUTF8(text.data());
    break;
  }
  case granit::input_event_type::pointer_moved:
    io.AddMousePosEvent(event.data.pointer_moved.x, event.data.pointer_moved.y);
    break;
  case granit::input_event_type::pointer_button: {
    io.AddMousePosEvent(event.data.pointer_button.x, event.data.pointer_button.y);
    const auto button = map_button(event.data.pointer_button.button);
    if (button >= 0)
      io.AddMouseButtonEvent(button, event.data.pointer_button.pressed != 0);
    break;
  }
  case granit::input_event_type::pointer_wheel:
    io.AddMouseWheelEvent(event.data.pointer_wheel.delta_x, event.data.pointer_wheel.delta_y);
    break;
  case granit::input_event_type::pointer_left:
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    break;
  case granit::input_event_type::none:
  case granit::input_event_type::pointer_entered:
    break;
  }
}

void begin_frame(const granit::window_state& state, float delta_seconds) noexcept {
  auto& io = ImGui::GetIO();
  io.DisplaySize = {static_cast<float>(state.width), static_cast<float>(state.height)};
  io.DisplayFramebufferScale = {
      state.width == 0
          ? 1.0F
          : static_cast<float>(state.framebuffer_width) / static_cast<float>(state.width),
      state.height == 0
          ? 1.0F
          : static_cast<float>(state.framebuffer_height) / static_cast<float>(state.height),
  };
  io.DeltaTime = delta_seconds > 0.0F ? delta_seconds : 1.0F / 60.0F;
  ImGui::NewFrame();
}

} // namespace granit::example::imgui
