// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_IMGUI_INPUT_H_
#define GRANIT_EXAMPLES_COMMON_IMGUI_INPUT_H_

#include <granit/window.hpp>

namespace granit::example::imgui {

/** 将 Granit 窗口事件转发给当前 ImGui 上下文。 */
void process_window_event(const granit::window_event& event) noexcept;

/** 将 Granit 输入事件转发给当前 ImGui 上下文。 */
void process_input_event(const granit::input_event& event) noexcept;

/** 根据窗口状态开始一帧 ImGui。 */
void begin_frame(const granit::window_state& state, float delta_seconds) noexcept;

} // namespace granit::example::imgui

#endif
