// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_DIRECTIONAL_SHADOW_H
#define GRANIT_LIGHTING_DIRECTIONAL_SHADOW_H

#include "math/math.h"
#include "render_graph/serial_graph.h"
#include "scene/multi_view_submission.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace granit::lighting {

struct directional_shadow_volume {
  math::float3 focus{};
  float half_width = 10.0F;
  float half_height = 10.0F;
  float near_plane = 0.1F;
  float far_plane = 100.0F;
  float light_distance = 50.0F;
};

struct alignas(16) shadow_frame_constants {
  math::matrix4 light_view_projection{};
};

struct shadow_caster {
  math::matrix4 model{};
  std::uint64_t payload = 0;
  std::uint32_t object_id = 0;
  std::uint32_t source_index = 0;
};

struct alignas(16) directional_shadow_pass_desc {
  shadow_frame_constants frame;
  std::vector<shadow_caster> casters;
  render_graph::resource_id depth = render_graph::invalid_resource_id;
  std::uint32_t reserved = 0;
};

enum class directional_shadow_error : std::uint8_t {
  none,
  view_out_of_range,
  light_out_of_range,
  light_not_visible,
  invalid_volume,
  invalid_frustum,
  invalid_depth,
  no_casters,
  invalid_callback,
  pass_rejected,
  out_of_memory,
};

/**
 * 使用显式方向光和正交体，从全部场景 Renderable 构建阴影投影者；失败时不修改 output。
 */
[[nodiscard]] directional_shadow_error build_directional_shadow_pass_desc(
    const scene::multi_view_snapshot& snapshot, std::size_t view_index,
    std::uint32_t directional_light_index, const directional_shadow_volume& volume,
    render_graph::resource_id depth, directional_shadow_pass_desc& output) noexcept;

using directional_shadow_record_callback = std::function<granit_result(
    render_graph::pass_context&, const shadow_frame_constants&, std::span<const shadow_caster>)>;

/** 添加只写深度资源的方向光 Shadow Pass；描述与回调由 Graph 持有。 */
[[nodiscard]] render_graph::pass_id add_directional_shadow_graph_pass(
    render_graph::serial_graph& graph, directional_shadow_pass_desc desc,
    directional_shadow_record_callback callback, std::string name = "Directional Shadow");

} // namespace granit::lighting

#endif
