// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_VIEWER_INPUT_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_VIEWER_INPUT_H_

namespace granit::example::model_viewer {

/** 平台壳提交给查看器 Core 的单帧输入。 */
struct viewer_input {
  float pointer_delta_x{};
  float pointer_delta_y{};
  float wheel_delta{};
  bool orbiting{};
  bool panning{};
  bool focus_requested{};
  bool home_requested{};
  bool mouse_captured{};
  bool keyboard_captured{};
  bool window_focused{true};
  bool pointer_inside{true};
};

} // namespace granit::example::model_viewer

#endif
