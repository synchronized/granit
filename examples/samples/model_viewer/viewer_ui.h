// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_UI_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_UI_H_

#include "imgui/frame_canvas_data.h"

#include <granit/granit.hpp>
#include <granit/window.hpp>
#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace granit::example::model_viewer {

/** Desktop/Web 共用的 ImGui Context、输入、纹理注册和 Draw Data 捕获。 */
class viewer_ui final {
public:
  viewer_ui();
  ~viewer_ui();
  viewer_ui(const viewer_ui&) = delete;
  viewer_ui& operator=(const viewer_ui&) = delete;

  [[nodiscard]] granit::result initialize() noexcept;
  void process(const granit::window_event& event) noexcept;
  void process(const granit::input_event& event) noexcept;
  void begin_frame(const granit::window_state& state, float delta_seconds) noexcept;
  [[nodiscard]] granit::result capture(imgui::frame_canvas_data& output) noexcept;

  [[nodiscard]] granit::result capture_font_atlas(std::vector<std::byte>& pixels,
                                                  std::uint32_t& width,
                                                  std::uint32_t& height) const;
  [[nodiscard]] granit::result register_font(granit::texture_view_ref view,
                                             granit::sampler_ref sampler) noexcept;
  [[nodiscard]] granit::result register_texture(granit::texture_view_ref view,
                                                granit::sampler_ref sampler,
                                                ImTextureID& texture) noexcept;
  [[nodiscard]] granit::result unregister_texture(ImTextureID texture) noexcept;
  void clear_textures() noexcept;

  [[nodiscard]] bool wants_mouse() const noexcept;
  [[nodiscard]] bool wants_keyboard() const noexcept;
  [[nodiscard]] bool valid() const noexcept;

private:
  struct state;
  std::unique_ptr<state> state_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_UI_H_
