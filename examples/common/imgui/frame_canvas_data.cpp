// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "imgui/frame_canvas_data.h"

namespace granit::example::imgui {

void frame_canvas_data::clear() noexcept {
  vertices.clear();
  indices.clear();
  ranges.clear();
}

granit::result frame_canvas_data::append_to(granit::canvas_draw_list& canvas) const noexcept {
  if (empty())
    return granit::result::success;
  return canvas.append_batch(vertices, indices, ranges);
}

} // namespace granit::example::imgui
