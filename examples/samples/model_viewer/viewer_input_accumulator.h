// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_INPUT_ACCUMULATOR_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_INPUT_ACCUMULATOR_H_

#include "model_viewer/viewer_input.h"

#include <granit/window.hpp>

#include <cstdint>

namespace granit::example::model_viewer {

/** 将统一 Window/Input 事件累积为 Viewer Core 消费的一帧输入。 */
class viewer_input_accumulator {
public:
  /** 标记应用帧边界；瞬时输入保留到 finish，以免在渲染背压期间丢失。 */
  void begin_frame() noexcept;
  void process(const granit::window_event& event) noexcept;
  void process(const granit::input_event& event, bool pointer_captured = false,
               bool keyboard_captured = false) noexcept;
  [[nodiscard]] viewer_input finish(bool pointer_captured = false,
                                    bool keyboard_captured = false) noexcept;

  /** 背压期间合并进后续提交帧的额外输入帧数。 */
  [[nodiscard]] std::uint32_t merged_input_frames() const noexcept { return merged_input_frames_; }

private:
  void clear_transient() noexcept;
  void clear_interaction() noexcept;

  viewer_input input_;
  bool orbit_motion_pending_{};
  bool pan_motion_pending_{};
  std::uint32_t begin_frame_count_{};
  std::uint32_t merged_input_frames_{};
};

} // namespace granit::example::model_viewer

#endif
