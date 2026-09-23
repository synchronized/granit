// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TUTORIAL_01_IMGUI_INPUT_HPP_
#define GRANIT_TUTORIAL_01_IMGUI_INPUT_HPP_

#include <granit/window.hpp>
#include <imgui.h>

namespace tutorial_imgui {

void process_window_event(const granit::window_event& event) noexcept;
void process_input_event(const granit::input_event& event) noexcept;
void begin_frame(const granit::window_state& state, float delta_seconds) noexcept;

} // namespace tutorial_imgui

#endif
