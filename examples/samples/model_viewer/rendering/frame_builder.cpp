// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/rendering/frame_builder.h"

#include "camera/orbit_camera_input_accumulator.h"
#include "model_viewer/ui/viewer_ui.h"

#include <utility>

namespace granit::example::model_viewer {

granit::result build_viewer_frame(viewer_document& document,
                                  const gltf_rendering::scene_plan& scene_plan, viewer_ui& ui,
                                  camera::orbit_camera_input_accumulator& input,
                                  const viewer_frame_build_desc& desc,
                                  viewer_frame_build_result& output) {
  viewer_frame_build_result candidate;
  if (desc.show_ui) {
    ui.begin_frame(desc.window, desc.delta_seconds);
    candidate.changes = draw_viewer_panels(document.scene(), document.state(), desc.renderer,
                                           desc.performance, desc.quality, desc.previews);
    const auto capture_result = ui.capture(candidate.packet.canvas);
    if (capture_result.failed())
      return capture_result;
  }

  viewer_document_update tick;
  tick.input = input.finish(desc.show_ui && ui.wants_mouse(), desc.show_ui && ui.wants_keyboard());
  candidate.input_applied = tick.input.pointer_delta_x != 0.0F ||
                            tick.input.pointer_delta_y != 0.0F || tick.input.wheel_delta != 0.0F ||
                            tick.input.focus_requested || tick.input.home_requested;
  tick.change = candidate.changes.state;
  tick.width = desc.renderer.width;
  tick.height = desc.renderer.height;
  tick.performance = desc.sample;
  viewer_document_frame document_frame;
  const auto operation = document.update(tick, scene_plan, document_frame);
  if (operation.failed())
    return operation;
  candidate.packet.viewer = {.view = document_frame.view,
                             .directional_light = document_frame.directional_light,
                             .width = tick.width,
                             .height = tick.height,
                             .exposure_ev = document_frame.exposure_ev,
                             .environment_intensity = document_frame.environment_intensity,
                             .environment_rotation_radians =
                                 document_frame.environment_rotation_radians,
                             .clear_color = document_frame.clear_color};
  output = std::move(candidate);
  return granit::result::success;
}

} // namespace granit::example::model_viewer
