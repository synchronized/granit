// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_RENDER_GRAPH_ADAPTER_H
#define GRANIT_MATERIAL_PBR_RENDER_GRAPH_ADAPTER_H

#include "material/pbr_draw_inputs.h"
#include "render_graph/serial_graph.h"

#include <functional>
#include <span>
#include <vector>

namespace granit::material {

struct pbr_graph_pass_desc {
  render_graph::resource_id color = render_graph::invalid_resource_id;
  render_graph::resource_id resolve_color = render_graph::invalid_resource_id;
  render_graph::resource_id depth = render_graph::invalid_resource_id;
  render_graph::resource_id shadow = render_graph::invalid_resource_id;
  pbr_view_input view;
  pbr_directional_light_input light;
  std::vector<pbr_object_input> objects;
};

using pbr_graph_record_callback =
    std::function<granit_result(render_graph::pass_context&, const pbr_frame_constants&,
                                std::span<const pbr_object_constants>)>;

/**
 * 向串行 Render Graph 添加一个 PBR Pass。适配器只打包输入并声明附件访问，实际 Draw 由回调录制。
 */
[[nodiscard]] render_graph::pass_id add_pbr_graph_pass(render_graph::serial_graph& graph,
                                                       pbr_graph_pass_desc desc,
                                                       pbr_graph_record_callback callback,
                                                       std::string name = "PBR Forward");

} // namespace granit::material

#endif
