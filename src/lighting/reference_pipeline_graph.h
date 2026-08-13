// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_REFERENCE_PIPELINE_GRAPH_H
#define GRANIT_LIGHTING_REFERENCE_PIPELINE_GRAPH_H

#include "lighting/directional_shadow.h"
#include "lighting/tone_mapping_pass.h"
#include "material/pbr_render_graph_adapter.h"

#include <optional>
#include <string>

namespace granit::lighting {

struct reference_pipeline_graph_desc {
  std::optional<directional_shadow_pass_desc> shadow;
  material::pbr_graph_pass_desc pbr;
  tone_mapping_graph_pass_desc tone_mapping;
};

struct reference_pipeline_graph_callbacks {
  directional_shadow_record_callback shadow;
  material::pbr_graph_record_callback pbr;
  tone_mapping_record_callback tone_mapping;
};

struct reference_pipeline_graph_passes {
  render_graph::pass_id shadow = render_graph::invalid_pass_id;
  render_graph::pass_id pbr = render_graph::invalid_pass_id;
  render_graph::pass_id tone_mapping = render_graph::invalid_pass_id;
};

enum class reference_pipeline_graph_error : std::uint8_t {
  none,
  invalid_resource,
  invalid_callback,
  inconsistent_resource,
  pass_rejected,
};

/**
 * 串联可选方向光阴影、PBR HDR 与 Tone Mapping；资源仍由调用方或 Graph 持有。
 */
[[nodiscard]] reference_pipeline_graph_error add_reference_pipeline_graph(
    render_graph::serial_graph& graph, reference_pipeline_graph_desc desc,
    reference_pipeline_graph_callbacks callbacks, reference_pipeline_graph_passes& output,
    std::string name_prefix = "Reference Pipeline");

} // namespace granit::lighting

#endif
