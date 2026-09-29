// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_FRAME_BUILDER_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_FRAME_BUILDER_H_

#include "camera/orbit_camera_input_accumulator.h"
#include "gltf_rendering/scene_plan.h"
#include "model_viewer/model/viewer_document.h"
#include "model_viewer/rendering/render_execution.h"
#include "model_viewer/ui/viewer_panels.h"

#include <granit/window.hpp>

#include <optional>
#include <span>

namespace granit::example::model_viewer {

class viewer_ui;

struct viewer_frame_build_desc {
  const granit::window_state& window;
  float delta_seconds{};
  renderer_panel_info renderer;
  performance_panel_info performance;
  render_quality_config quality;
  std::span<const texture_preview> previews;
  std::optional<performance_sample> sample;
  bool show_ui{true};
};

struct viewer_frame_build_result {
  frame_packet packet;
  viewer_panel_changes changes;
  bool input_applied{};
};

/** 统一消费输入、构建 Viewer/ImGui 帧，并返回尚未执行的 GPU 配置变更。 */
[[nodiscard]] granit::result
build_viewer_frame(viewer_document& document, const gltf_rendering::scene_plan& scene_plan,
                   viewer_ui& ui, camera::orbit_camera_input_accumulator& input,
                   const viewer_frame_build_desc& desc, viewer_frame_build_result& output);

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_FRAME_BUILDER_H_
