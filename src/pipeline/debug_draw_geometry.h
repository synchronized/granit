// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DEBUG_DRAW_GEOMETRY_H_
#define GRANIT_PIPELINE_DEBUG_DRAW_GEOMETRY_H_

#include <granit/math/types.h>
#include <granit/pipeline/debug_draw_list.h>

#include <array>
#include <cstdint>

namespace granit::pipeline::detail {

struct debug_clip_vertex {
  std::array<float, 4> position{};
  std::uint32_t color = 0;
};

enum class debug_line_expand_result : std::uint8_t {
  success,
  clipped,
  invalid_argument,
};

/** 将世界线段裁剪到 Vulkan clip volume，并展开为恒定像素宽度的四边形。 */
[[nodiscard]] GRANIT_RENDER_PIPELINE_API debug_line_expand_result
expand_world_debug_line(const granit_debug_draw_line& line, const granit_matrix4& view_projection,
                        std::uint32_t viewport_width, std::uint32_t viewport_height,
                        std::array<debug_clip_vertex, 4>& output) noexcept;

} // namespace granit::pipeline::detail

#endif
