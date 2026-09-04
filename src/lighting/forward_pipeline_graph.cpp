// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/forward_pipeline_graph.h"

#include <utility>

namespace granit::lighting {

forward_pipeline_graph_error add_forward_pipeline_graph(render_graph::serial_graph& graph,
                                                        forward_pipeline_graph_desc desc,
                                                        forward_pipeline_graph_callbacks callbacks,
                                                        forward_pipeline_graph_passes& output,
                                                        std::string name_prefix) {
  const auto invalid_resource = render_graph::invalid_resource_id;
  if (desc.pbr.color == invalid_resource || desc.pbr.depth == invalid_resource ||
      desc.tone_mapping.hdr_color == invalid_resource ||
      desc.tone_mapping.output == invalid_resource) {
    return forward_pipeline_graph_error::invalid_resource;
  }
  if (!callbacks.pbr || !callbacks.tone_mapping || (desc.shadow && !callbacks.shadow))
    return forward_pipeline_graph_error::invalid_callback;
  const auto pbr_output = desc.pbr.resolve_color == render_graph::invalid_resource_id
                              ? desc.pbr.color
                              : desc.pbr.resolve_color;
  if (pbr_output != desc.tone_mapping.hdr_color)
    return forward_pipeline_graph_error::inconsistent_resource;
  if (desc.shadow &&
      (desc.shadow->depth == invalid_resource || desc.pbr.shadow != desc.shadow->depth)) {
    return forward_pipeline_graph_error::inconsistent_resource;
  }
  forward_pipeline_graph_passes passes;
  if (desc.shadow) {
    passes.shadow = add_directional_shadow_graph_pass(
        graph, std::move(*desc.shadow), std::move(callbacks.shadow), name_prefix + " / Shadow");
    if (passes.shadow == render_graph::invalid_pass_id)
      return forward_pipeline_graph_error::pass_rejected;
  }
  passes.pbr = material::add_pbr_graph_pass(graph, std::move(desc.pbr), std::move(callbacks.pbr),
                                            name_prefix + " / PBR HDR");
  if (passes.pbr == render_graph::invalid_pass_id)
    return forward_pipeline_graph_error::pass_rejected;
  passes.tone_mapping = add_tone_mapping_graph_pass(graph, std::move(desc.tone_mapping),
                                                    std::move(callbacks.tone_mapping),
                                                    name_prefix + " / Tone Mapping");
  if (passes.tone_mapping == render_graph::invalid_pass_id)
    return forward_pipeline_graph_error::pass_rejected;

  output = passes;
  return forward_pipeline_graph_error::none;
}

} // namespace granit::lighting
