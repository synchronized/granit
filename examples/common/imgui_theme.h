// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_IMGUI_THEME_H_
#define GRANIT_EXAMPLES_COMMON_IMGUI_THEME_H_

#include <imgui.h>

#include <cmath>

namespace granit::example {

inline void apply_imgui_theme() {
  ImGui::StyleColorsDark();
  auto& style = ImGui::GetStyle();
  style.WindowPadding = {12, 10};
  style.FramePadding = {9, 5};
  style.ItemSpacing = {8, 7};
  style.ItemInnerSpacing = {7, 5};
  style.WindowRounding = 8;
  style.ChildRounding = 6;
  style.FrameRounding = 5;
  style.PopupRounding = 6;
  style.ScrollbarRounding = 9;
  style.GrabRounding = 5;
  style.TabRounding = 5;
  style.WindowBorderSize = 1;
  style.FrameBorderSize = 0;
  style.ScrollbarSize = 13;
  style.GrabMinSize = 10;

  auto* colors = style.Colors;
  colors[ImGuiCol_Text] = {0.88F, 0.91F, 0.96F, 1};
  colors[ImGuiCol_TextDisabled] = {0.47F, 0.52F, 0.63F, 1};
  colors[ImGuiCol_WindowBg] = {0.055F, 0.067F, 0.10F, 0.98F};
  colors[ImGuiCol_ChildBg] = {0.071F, 0.086F, 0.125F, 1};
  colors[ImGuiCol_PopupBg] = {0.071F, 0.086F, 0.125F, 0.98F};
  colors[ImGuiCol_Border] = {0.18F, 0.21F, 0.30F, 1};
  colors[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
  colors[ImGuiCol_FrameBg] = {0.105F, 0.125F, 0.18F, 1};
  colors[ImGuiCol_FrameBgHovered] = {0.15F, 0.18F, 0.27F, 1};
  colors[ImGuiCol_FrameBgActive] = {0.19F, 0.22F, 0.33F, 1};
  colors[ImGuiCol_TitleBg] = {0.071F, 0.086F, 0.125F, 1};
  colors[ImGuiCol_TitleBgActive] = {0.105F, 0.125F, 0.18F, 1};
  colors[ImGuiCol_TitleBgCollapsed] = {0.055F, 0.067F, 0.10F, 0.92F};
  colors[ImGuiCol_MenuBarBg] = {0.071F, 0.086F, 0.125F, 1};
  colors[ImGuiCol_ScrollbarBg] = {0.055F, 0.067F, 0.10F, 0.75F};
  colors[ImGuiCol_ScrollbarGrab] = {0.20F, 0.23F, 0.34F, 1};
  colors[ImGuiCol_ScrollbarGrabHovered] = {0.29F, 0.33F, 0.48F, 1};
  colors[ImGuiCol_ScrollbarGrabActive] = {0.43F, 0.36F, 0.86F, 1};
  colors[ImGuiCol_CheckMark] = {0.52F, 0.45F, 0.96F, 1};
  colors[ImGuiCol_SliderGrab] = {0.43F, 0.36F, 0.86F, 1};
  colors[ImGuiCol_SliderGrabActive] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_Button] = {0.36F, 0.29F, 0.76F, 1};
  colors[ImGuiCol_ButtonHovered] = {0.46F, 0.38F, 0.92F, 1};
  colors[ImGuiCol_ButtonActive] = {0.55F, 0.47F, 1, 1};
  colors[ImGuiCol_Header] = {0.27F, 0.23F, 0.52F, 0.72F};
  colors[ImGuiCol_HeaderHovered] = {0.38F, 0.31F, 0.75F, 0.82F};
  colors[ImGuiCol_HeaderActive] = {0.46F, 0.38F, 0.92F, 1};
  colors[ImGuiCol_Separator] = {0.18F, 0.21F, 0.30F, 1};
  colors[ImGuiCol_SeparatorHovered] = {0.43F, 0.36F, 0.86F, 1};
  colors[ImGuiCol_SeparatorActive] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_ResizeGrip] = {0.43F, 0.36F, 0.86F, 0.25F};
  colors[ImGuiCol_ResizeGripHovered] = {0.52F, 0.45F, 0.96F, 0.70F};
  colors[ImGuiCol_ResizeGripActive] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_TextSelectedBg] = {0.43F, 0.36F, 0.86F, 0.38F};
  colors[ImGuiCol_NavHighlight] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_ModalWindowDimBg] = {0.02F, 0.025F, 0.04F, 0.72F};

  // ImGui 色板按显示空间书写；Canvas 顶点色使用线性空间。
  const auto to_linear = [](float value) {
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
  };
  for (int index = 0; index < ImGuiCol_COUNT; ++index) {
    auto& color = colors[index];
    color.x = to_linear(color.x);
    color.y = to_linear(color.y);
    color.z = to_linear(color.z);
  }
}

} // namespace granit::example

#endif
