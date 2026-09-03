// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_pass.h"

#include <cmath>
#include <utility>

namespace granit::lighting {

granit_texture_desc make_hdr_attachment_desc(std::uint32_t width, std::uint32_t height) noexcept {
  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT;
  desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  desc.width = width;
  desc.height = height;
  return desc;
}

render_graph::pass_id add_tone_mapping_graph_pass(render_graph::serial_graph& graph,
                                                  tone_mapping_graph_pass_desc desc,
                                                  tone_mapping_record_callback callback,
                                                  std::string name) {
  if (desc.hdr_color == render_graph::invalid_resource_id ||
      desc.output == render_graph::invalid_resource_id || desc.hdr_color == desc.output ||
      !callback || !std::isfinite(desc.tone_mapping.exposure_ev) ||
      desc.tone_mapping.exposure_ev < tone_mapping_min_exposure_ev ||
      desc.tone_mapping.exposure_ev > tone_mapping_max_exposure_ev ||
      validate_tone_mapping_output(desc.output_format, desc.tone_mapping.output_transfer) !=
          tone_mapping_error::none) {
    return render_graph::invalid_pass_id;
  }

  const tone_mapping_constants constants{
      .exposure_scale = std::exp2(desc.tone_mapping.exposure_ev),
      .encode_srgb = desc.tone_mapping.output_transfer == tone_mapping_output_transfer::shader_srgb
                         ? UINT32_C(1)
                         : UINT32_C(0),
      .enable_fxaa = desc.tone_mapping.enable_fxaa ? UINT32_C(1) : UINT32_C(0)};
  render_graph::pass_desc pass{.side_effect = true,
                               .accesses = {{desc.hdr_color, render_graph::access_type::read},
                                            {desc.output, render_graph::access_type::write}}};
  return graph.add_pass(
      std::move(pass),
      [constants, callback = std::move(callback)](render_graph::pass_context& context) {
        return callback(context, constants);
      },
      std::move(name));
}

} // namespace granit::lighting
