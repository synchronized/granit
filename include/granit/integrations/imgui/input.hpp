// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INTEGRATIONS_IMGUI_INPUT_HPP_
#define GRANIT_INTEGRATIONS_IMGUI_INPUT_HPP_

#include <imgui.h>

#include <granit/integrations/imgui/export.hpp>

namespace granit::integration::imgui {

/** 描述一次 ImGui 新帧所需的显示尺寸、DPI 缩放和时间步长。 */
struct frame_desc {
  float display_width{};
  float display_height{};
  float framebuffer_scale_x{1.0F};
  float framebuffer_scale_y{1.0F};
  float delta_seconds{1.0F / 60.0F};
};

/** 将帧信息写入当前 ImGui Context 并开始新帧；Context 由调用方拥有。 */
inline void begin_frame(const frame_desc& frame) noexcept {
  auto& io = ImGui::GetIO();
  io.DisplaySize = {frame.display_width, frame.display_height};
  io.DisplayFramebufferScale = {frame.framebuffer_scale_x, frame.framebuffer_scale_y};
  io.DeltaTime = frame.delta_seconds > 0.0F ? frame.delta_seconds : 1.0F / 60.0F;
  ImGui::NewFrame();
}

} // namespace granit::integration::imgui

#endif
