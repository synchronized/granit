// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_ui.h"

#include "imgui/imgui_frame_capture.h"
#include "imgui/imgui_input.h"
#include "imgui/imgui_texture_registry.h"
#include "imgui/imgui_theme.h"

#include <limits>
#include <new>

namespace granit::example::model_viewer {

struct viewer_ui::state {
  imgui::texture_registry textures;
};

viewer_ui::viewer_ui() = default;

viewer_ui::~viewer_ui() {
  if (state_) {
    state_->textures.clear();
    ImGui::DestroyContext();
  }
}

granit::result viewer_ui::initialize() noexcept {
  if (state_ != nullptr || ImGui::GetCurrentContext() != nullptr)
    return granit::result::invalid_argument;
  try {
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr)
      return granit::result::out_of_memory;
    auto state = std::make_unique<viewer_ui::state>();
    ImGui::GetIO().IniFilename = nullptr;
    granit::example::apply_imgui_theme();
    state_ = std::move(state);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    ImGui::DestroyContext();
    return granit::result::out_of_memory;
  } catch (...) {
    ImGui::DestroyContext();
    return granit::result::internal;
  }
}

void viewer_ui::process(const granit::window_event& event) noexcept {
  if (state_)
    imgui::process_window_event(event);
}

void viewer_ui::process(const granit::input_event& event) noexcept {
  if (state_)
    imgui::process_input_event(event);
}

void viewer_ui::begin_frame(const granit::window_state& state, float delta_seconds) noexcept {
  if (state_)
    imgui::begin_frame(state, delta_seconds);
}

granit::result viewer_ui::capture(imgui::frame_canvas_data& output) noexcept {
  if (!state_)
    return granit::result::not_ready;
  ImGui::Render();
  return imgui::capture_imgui_frame(ImGui::GetDrawData(), imgui::texture_registry::resolver,
                                    &state_->textures, output);
}

granit::result viewer_ui::capture_font_atlas(std::vector<std::byte>& pixels,
                                             std::uint32_t& output_width,
                                             std::uint32_t& output_height) const {
  if (!state_)
    return granit::result::not_ready;
  unsigned char* source{};
  int width{};
  int height{};
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&source, &width, &height);
  if (source == nullptr || width <= 0 || height <= 0)
    return granit::result::internal;
  const auto pixel_count = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (pixel_count > std::numeric_limits<std::size_t>::max() / 4)
    return granit::result::out_of_memory;
  try {
    pixels.resize(static_cast<std::size_t>(pixel_count) * 4);
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
  for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
    const auto alpha = source[offset + 3];
    for (std::size_t channel = 0; channel < 3; ++channel) {
      pixels[offset + channel] = static_cast<std::byte>(
          (static_cast<std::uint32_t>(source[offset + channel]) * alpha + 127U) / 255U);
    }
    pixels[offset + 3] = static_cast<std::byte>(alpha);
  }
  output_width = static_cast<std::uint32_t>(width);
  output_height = static_cast<std::uint32_t>(height);
  return granit::result::success;
}

granit::result viewer_ui::register_font(granit::texture_view_ref view,
                                        granit::sampler_ref sampler) noexcept {
  ImTextureID texture = ImTextureID_Invalid;
  const auto result = register_texture(view, sampler, texture);
  if (result.ok()) {
    ImGui::GetIO().Fonts->SetTexID(texture);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
  }
  return result;
}

granit::result viewer_ui::register_texture(granit::texture_view_ref view,
                                           granit::sampler_ref sampler,
                                           ImTextureID& texture) noexcept {
  return state_ ? state_->textures.register_texture(view, sampler, texture)
                : granit::result::not_ready;
}

granit::result viewer_ui::unregister_texture(ImTextureID texture) noexcept {
  return state_ ? state_->textures.unregister_texture(texture) : granit::result::not_ready;
}

void viewer_ui::clear_textures() noexcept {
  if (state_)
    state_->textures.clear();
}

bool viewer_ui::wants_mouse() const noexcept { return state_ && ImGui::GetIO().WantCaptureMouse; }

bool viewer_ui::wants_keyboard() const noexcept {
  return state_ && ImGui::GetIO().WantCaptureKeyboard;
}

bool viewer_ui::valid() const noexcept { return state_ != nullptr; }

} // namespace granit::example::model_viewer
