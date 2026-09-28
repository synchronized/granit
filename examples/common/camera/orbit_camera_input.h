// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_CAMERA_ORBIT_CAMERA_INPUT_H_
#define GRANIT_EXAMPLES_COMMON_CAMERA_ORBIT_CAMERA_INPUT_H_

namespace granit::example::camera {

/** 一帧轨道相机输入；捕获标志用于与 UI 安全共享输入。 */
struct orbit_camera_input {
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

} // namespace granit::example::camera

#endif
