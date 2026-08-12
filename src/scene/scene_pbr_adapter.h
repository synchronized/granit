// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SCENE_SCENE_PBR_ADAPTER_H
#define GRANIT_SCENE_SCENE_PBR_ADAPTER_H

#include "material/pbr_render_graph_adapter.h"
#include "scene/multi_view_submission.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace granit::scene {

enum class scene_pbr_error : std::uint8_t {
  none,
  invalid_view,
  no_visible_renderables,
  directional_light_count,
  invalid_attachment,
  invalid_callback,
  out_of_memory,
  pass_rejected,
};

struct scene_pbr_pass_desc {
  material::pbr_graph_pass_desc pbr;
  std::vector<std::uint32_t> renderable_indices;
};

using scene_pbr_record_callback = std::function<granit_result(
    render_graph::pass_context&, const material::pbr_frame_constants&,
    std::span<const material::pbr_object_constants>, std::span<const std::uint32_t>)>;

/** 将指定 View 的可见结果转换为 H-03 PBR Pass 描述，失败时不修改 output。 */
[[nodiscard]] scene_pbr_error build_scene_pbr_pass_desc(const multi_view_snapshot& snapshot,
                                                        std::size_t view_index,
                                                        render_graph::resource_id color,
                                                        render_graph::resource_id depth,
                                                        scene_pbr_pass_desc& output) noexcept;

/** 转换指定 View 并向 Render Graph 添加 PBR Pass；实际 Draw 仍由 callback 录制。 */
[[nodiscard]] render_graph::pass_id
add_scene_pbr_graph_pass(render_graph::serial_graph& graph, const multi_view_snapshot& snapshot,
                         std::size_t view_index, render_graph::resource_id color,
                         render_graph::resource_id depth, scene_pbr_record_callback callback,
                         scene_pbr_error& error, std::string name = "Scene PBR Forward");

} // namespace granit::scene

#endif
