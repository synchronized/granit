// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_DESKTOP_SDL3_INPUT_H_
#define GRANIT_EXAMPLES_PLATFORM_DESKTOP_SDL3_INPUT_H_

#include "model_viewer/orbit_camera.h"

#include <SDL3/SDL_events.h>

namespace granit::example::model_viewer::desktop {

/** 将 SDL3 事件累积为一帧后端无关的查看器输入。 */
class sdl3_input {
public:
  void begin_frame() noexcept;
  void process(const SDL_Event& event) noexcept;
  [[nodiscard]] viewer_input finish(bool mouse_captured, bool keyboard_captured) const noexcept;

private:
  viewer_input input_;
};

} // namespace granit::example::model_viewer::desktop

#endif
