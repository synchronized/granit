// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_FRAME_CANVAS_DATA_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_FRAME_CANVAS_DATA_H_

#include <granit/pipeline/canvas_draw_list.hpp>

#include <cstdint>
#include <vector>

namespace granit::example::model_viewer {

/** 单帧自有的 Canvas CPU 数据，可安全移动到另一线程后再写入 GPU Canvas。 */
struct frame_canvas_data {
  std::vector<granit_canvas_vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<granit_canvas_draw_range> ranges;

  [[nodiscard]] bool empty() const noexcept { return ranges.empty(); }
  void clear() noexcept;
  [[nodiscard]] granit::result append_to(granit::canvas_draw_list& canvas) const noexcept;
};

} // namespace granit::example::model_viewer

#endif
