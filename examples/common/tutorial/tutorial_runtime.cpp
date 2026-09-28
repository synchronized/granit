// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "tutorial/tutorial_runtime.h"

#include "imgui/imgui_font_atlas.h"
#include "imgui/imgui_input.h"
#include "imgui/imgui_theme.h"

#include <granit/integrations/imgui/renderer.hpp>
#include <imgui.h>

#include <algorithm>

namespace granit::example::tutorial {

tutorial_runtime::~tutorial_runtime() { shutdown(); }

result tutorial_runtime::initialize(renderer& renderer) noexcept {
  if (initialized_)
    return result::invalid_argument;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  apply_imgui_theme();

  auto operation = canvas_.initialize(renderer);
  if (operation.ok()) {
    operation = imgui::initialize_font_atlas(renderer, textures_, font_texture_, font_view_,
                                             font_sampler_);
  }
  if (operation.failed()) {
    shutdown();
    return operation;
  }
  initialized_ = true;
  return result::success;
}

void tutorial_runtime::process(const window_event& event) noexcept {
  if (initialized_)
    imgui::process_window_event(event);
}

void tutorial_runtime::process(const input_event& event) noexcept {
  if (initialized_)
    imgui::process_input_event(event);
}

result tutorial_runtime::begin_frame(const window_state& state, float delta_seconds,
                                     const tutorial_info& info) noexcept {
  if (!initialized_ || frame_open_ || state.framebuffer_width == 0 ||
      state.framebuffer_height == 0) {
    return result::invalid_argument;
  }

  const auto safe_delta = std::max(delta_seconds, 0.000001F);
  smoothed_delta_seconds_ = smoothed_delta_seconds_ == 0.0F
                                ? safe_delta
                                : smoothed_delta_seconds_ * 0.9F + safe_delta * 0.1F;
  smoothed_fps_ = 1.0F / smoothed_delta_seconds_;

  imgui::begin_frame(state, safe_delta);
  ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({340, 0}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Granit Tutorial");
  ImGui::TextUnformatted(info.name.data(), info.name.data() + info.name.size());
  ImGui::TextUnformatted(info.description.data(), info.description.data() + info.description.size());
  ImGui::Separator();
  ImGui::Text("FPS: %.1f | CPU frame: %.2f ms", smoothed_fps_,
              smoothed_delta_seconds_ * 1000.0F);
  ImGui::Text("Framebuffer: %u x %u | Frame: %u", state.framebuffer_width,
              state.framebuffer_height, info.frame);
  ImGui::SeparatorText("Chapter Settings");
  frame_open_ = true;
  return result::success;
}

result tutorial_runtime::end_frame() noexcept {
  if (!initialized_ || !frame_open_)
    return result::invalid_argument;
  ImGui::End();
  ImGui::Render();
  frame_open_ = false;

  auto operation = canvas_.clear();
  if (operation.ok()) {
    operation = integration::imgui::append_draw_data(
        ImGui::GetDrawData(), canvas_, imgui::texture_registry::resolver, &textures_);
  }
  canvas_draw_list_stats stats{};
  if (operation.ok())
    operation = canvas_.get_stats(stats);
  if (operation.ok())
    canvas_items_ = stats.item_count;
  return operation;
}

void tutorial_runtime::shutdown() noexcept {
  if (frame_open_ && ImGui::GetCurrentContext() != nullptr) {
    ImGui::End();
    frame_open_ = false;
  }
  textures_.clear();
  static_cast<void>(canvas_.destroy());
  static_cast<void>(font_sampler_.reset());
  static_cast<void>(font_view_.reset());
  static_cast<void>(font_texture_.reset());
  if (ImGui::GetCurrentContext() != nullptr)
    ImGui::DestroyContext();
  initialized_ = false;
  canvas_items_ = 0;
}

result tutorial_runtime::register_texture(texture_view_ref view, sampler_ref sampler,
                                          ImTextureID& texture) noexcept {
  return initialized_ ? textures_.register_texture(view, sampler, texture)
                      : result::invalid_argument;
}

result tutorial_runtime::unregister_texture(ImTextureID texture) noexcept {
  return initialized_ ? textures_.unregister_texture(texture) : result::invalid_argument;
}

bool tutorial_runtime::wants_mouse() const noexcept {
  return initialized_ && ImGui::GetIO().WantCaptureMouse;
}

bool tutorial_runtime::wants_keyboard() const noexcept {
  return initialized_ && ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace granit::example::tutorial
