// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_frame_builder.h"

#include "model_viewer/viewer_input_accumulator.h"
#include "model_viewer/viewer_session.h"
#include "model_viewer/viewer_ui.h"

#include <utility>

namespace granit::example::model_viewer {

granit::result build_viewer_frame(viewer_session& session, viewer_ui& ui,
                                  viewer_input_accumulator& input,
                                  const viewer_frame_build_desc& desc,
                                  viewer_frame_build_result& output) {
  viewer_frame_build_result candidate;
  if (desc.show_ui) {
    ui.begin_frame(desc.window, desc.delta_seconds);
    candidate.changes = draw_viewer_panels(session.cpu_scene(), session.state(), desc.renderer,
                                           desc.performance, desc.quality, desc.previews);
    const auto capture_result = ui.capture(candidate.packet.canvas);
    if (capture_result.failed())
      return capture_result;
  }

  application_tick_input tick;
  tick.input = input.finish(desc.show_ui && ui.wants_mouse(), desc.show_ui && ui.wants_keyboard());
  candidate.input_applied = tick.input.pointer_delta_x != 0.0F ||
                            tick.input.pointer_delta_y != 0.0F || tick.input.wheel_delta != 0.0F ||
                            tick.input.focus_requested || tick.input.home_requested;
  tick.change = candidate.changes.state;
  tick.width = desc.renderer.width;
  tick.height = desc.renderer.height;
  tick.performance = desc.sample;
  const auto operation = session.tick(tick, candidate.packet.viewer);
  if (operation.failed())
    return operation;
  output = std::move(candidate);
  return granit::result::success;
}

} // namespace granit::example::model_viewer
