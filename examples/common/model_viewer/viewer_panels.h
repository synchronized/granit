// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_VIEWER_PANELS_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_VIEWER_PANELS_H_

#include "model_viewer/material_edit.h"
#include "model_viewer/viewer_state.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace granit::example::model_viewer {

struct renderer_panel_info {
  std::string_view backend;
  std::string_view adapter;
  std::string_view swapchain_format;
  std::string_view present_mode;
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t frame_slots{};
};

struct performance_panel_info {
  float frames_per_second{};
  float cpu_frame_ms{};
  float frame_slot_wait_ms{};
  float present_wait_ms{};
  float gpu_frame_ms{};
  bool gpu_timing_available{};
};

struct viewer_panel_changes {
  viewer_change state;
  std::optional<material_factor_edit> material;
};

/** 构建查看器面板，并返回本帧由控件产生的批量状态变更。 */
[[nodiscard]] viewer_panel_changes draw_viewer_panels(const gltf::scene& scene,
                                                      const viewer_state& state,
                                                      const renderer_panel_info& renderer,
                                                      const performance_panel_info& performance);

} // namespace granit::example::model_viewer

#endif
