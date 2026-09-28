// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_TUTORIAL_RUNTIME_H_
#define GRANIT_EXAMPLES_COMMON_TUTORIAL_RUNTIME_H_

#include "imgui/imgui_texture_registry.h"

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/window.hpp>

#include <cstdint>
#include <string_view>

namespace granit::example::tutorial {

struct tutorial_info {
  std::string_view name;
  std::string_view description;
  std::uint32_t frame{};
};

/** 示例教程共享的 ImGui、字体、输入、Canvas 和帧指标运行时。 */
class tutorial_runtime {
public:
  tutorial_runtime() = default;
  ~tutorial_runtime();

  tutorial_runtime(const tutorial_runtime&) = delete;
  tutorial_runtime& operator=(const tutorial_runtime&) = delete;

  [[nodiscard]] result initialize(renderer& renderer) noexcept;
  void process(const window_event& event) noexcept;
  void process(const input_event& event) noexcept;

  /** 开始公共面板；调用方随后追加章节控件，并以 end_frame 结束。 */
  [[nodiscard]] result begin_frame(const window_state& state, float delta_seconds,
                                   const tutorial_info& info) noexcept;
  [[nodiscard]] result end_frame() noexcept;
  void shutdown() noexcept;

  [[nodiscard]] result register_texture(texture_view_ref view, sampler_ref sampler,
                                        ImTextureID& texture) noexcept;
  [[nodiscard]] result unregister_texture(ImTextureID texture) noexcept;

  [[nodiscard]] canvas_draw_list& canvas() noexcept { return canvas_; }
  [[nodiscard]] canvas_draw_list_ref canvas_ref() const noexcept { return canvas_.ref(); }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return canvas_items_; }
  [[nodiscard]] float frames_per_second() const noexcept { return smoothed_fps_; }
  [[nodiscard]] bool wants_mouse() const noexcept;
  [[nodiscard]] bool wants_keyboard() const noexcept;
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
  texture font_texture_;
  texture_view font_view_;
  sampler font_sampler_;
  canvas_draw_list canvas_;
  imgui::texture_registry textures_;
  float smoothed_delta_seconds_{};
  float smoothed_fps_{};
  std::uint32_t canvas_items_{};
  bool initialized_{};
  bool frame_open_{};
};

} // namespace granit::example::tutorial

#endif
