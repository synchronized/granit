// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_DESKTOP_SDL3_INPUT_H_
#define GRANIT_EXAMPLES_PLATFORM_DESKTOP_SDL3_INPUT_H_

#include "model_viewer/viewer_input.h"

#include <SDL3/SDL_events.h>

namespace granit::example::model_viewer::desktop {

/** 将 SDL3 事件累积为一帧后端无关的查看器输入。 */
class sdl3_input {
public:
  /** 保留帧边界入口；瞬时输入只在 finish 后清空，以跨过构造背压。 */
  void begin_frame() noexcept;
  void process(const SDL_Event& event, bool mouse_captured = false,
               bool keyboard_captured = false) noexcept;
  [[nodiscard]] viewer_input finish(bool mouse_captured, bool keyboard_captured) noexcept;

private:
  void clear_transient() noexcept;

  viewer_input input_;
  bool orbit_motion_pending_{};
  bool pan_motion_pending_{};
};

} // namespace granit::example::model_viewer::desktop

#endif
