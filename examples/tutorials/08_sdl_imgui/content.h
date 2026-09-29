// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_08_SDL_IMGUI_CONTENT_H_
#define GRANIT_EXAMPLES_TUTORIALS_08_SDL_IMGUI_CONTENT_H_

#include <imgui.h>

#include <cstdint>

namespace granit::example {

/** 固定验收画面的交互状态。 */
struct imgui_validation_state {
  bool validation_overlay{true};
};

/** 每帧由平台壳层提供给示例内容的只读信息。 */
struct imgui_sample_frame_info {
  std::uint32_t framebuffer_width{};
  std::uint32_t framebuffer_height{};
  const char* presentation{"Unknown"};
  bool show_custom_texture{};
  ImTextureID custom_texture{};
};

/** 固定逻辑坐标的验收画面；桌面与浏览器共用，不包含计时或平台文字。 */
inline void build_imgui_validation_scene(imgui_validation_state& state, ImTextureID texture) {
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize({320, 240});
  constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
  ImGui::Begin("Granit validation", nullptr, flags);
  auto* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled({0, 0}, {320, 240}, IM_COL32(0, 0, 0, 255));
  draw->AddText({24, 24}, IM_COL32_WHITE, "Granit ImGui 0123456789");
  draw->AddImage(ImTextureRef{texture}, {24, 64}, {88, 128});
  draw->PushClipRect({140, 64}, {204, 128}, true);
  draw->AddRectFilled({124, 48}, {220, 144}, IM_COL32(255, 0, 0, 255));
  draw->PopClipRect();
  ImGui::SetCursorScreenPos({24, 160});
  if (ImGui::InvisibleButton("validation-toggle", {160, 32}))
    state.validation_overlay = !state.validation_overlay;
  draw->AddRectFilled({24, 160}, {184, 192},
                      state.validation_overlay ? IM_COL32(0, 255, 0, 255)
                                               : IM_COL32(255, 0, 0, 255));
  ImGui::End();
}

/**
 * 构建一帧与平台无关的 ImGui 示例内容。
 *
 * 调用方负责 ImGui NewFrame/Render、输入转发以及 Draw Data 的 GPU 提交。
 */
inline void build_imgui_sample(const imgui_sample_frame_info& frame) {
  constexpr auto panel_flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::Begin("Granit Integration", nullptr, panel_flags);
  ImGui::TextUnformatted("SDL3 owns input; Granit Canvas renders ImGui on the selected backend.");
  ImGui::Text("Framebuffer: %u x %u", frame.framebuffer_width, frame.framebuffer_height);
  ImGui::Text("Presentation: %s", frame.presentation);
  ImGui::Text("Frame rate: %.1f FPS", ImGui::GetIO().Framerate);
  ImGui::Separator();
  if (frame.show_custom_texture) {
    ImGui::TextUnformatted("Custom Texture ID:");
    ImGui::Image(ImTextureRef{frame.custom_texture}, {64, 64});
  }
  ImGui::End();
}

} // namespace granit::example

#endif
