// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_WEB_WEB_INPUT_H_
#define GRANIT_EXAMPLES_PLATFORM_WEB_WEB_INPUT_H_

#include "model_viewer/viewer_input.h"

namespace granit::example::model_viewer::web {

enum class pointer_button { primary, middle, secondary };
enum class shortcut_key { other, focus, home };

/** 将浏览器事件中的稳定值累积为一帧后端无关输入。 */
class web_input {
public:
  void begin_frame() noexcept;
  void pointer_motion(float delta_x, float delta_y) noexcept;
  void pointer_button_changed(pointer_button button, bool pressed) noexcept;
  void wheel(float delta_y) noexcept;
  void key_pressed(shortcut_key key, bool repeat) noexcept;
  void focus_changed(bool focused) noexcept;
  void pointer_presence_changed(bool inside) noexcept;
  [[nodiscard]] viewer_input finish(bool mouse_captured, bool keyboard_captured) const noexcept;

private:
  viewer_input input_;
};

} // namespace granit::example::model_viewer::web

#endif
