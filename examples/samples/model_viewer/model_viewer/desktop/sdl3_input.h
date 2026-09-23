// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_DESKTOP_SDL3_INPUT_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_DESKTOP_SDL3_INPUT_H_

#include "model_viewer/viewer_input.h"

#include <SDL3/SDL_events.h>

#include <cstdint>

namespace granit::example::model_viewer::desktop {

/** 将 SDL3 事件累积为一帧后端无关的查看器输入。 */
class sdl3_input {
public:
  /** 保留帧边界入口；瞬时输入只在 finish 后清空，以跨过构造背压。 */
  void begin_frame() noexcept;
  void process(const SDL_Event& event, bool mouse_captured = false,
               bool keyboard_captured = false) noexcept;
  [[nodiscard]] viewer_input finish(bool mouse_captured, bool keyboard_captured) noexcept;
  /** 背压期间跨过多次帧边界、被合并进单个提交帧的输入增量次数。 */
  [[nodiscard]] std::uint32_t merged_input_frames() const noexcept { return merged_input_frames_; }

private:
  void clear_transient() noexcept;

  viewer_input input_;
  bool orbit_motion_pending_{};
  bool pan_motion_pending_{};
  std::uint32_t begin_frame_count_{};
  std::uint32_t merged_input_frames_{};
};

} // namespace granit::example::model_viewer::desktop

#endif
