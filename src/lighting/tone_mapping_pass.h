// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_TONE_MAPPING_PASS_H
#define GRANIT_LIGHTING_TONE_MAPPING_PASS_H

#include "lighting/tone_mapping_reference.h"
#include "render_graph/serial_graph.h"

#include <array>
#include <functional>
#include <string>

namespace granit::lighting {

struct alignas(16) tone_mapping_constants {
  float exposure_scale = 1.0F;
  std::uint32_t encode_srgb = 0;
  float inverse_width{};
  float inverse_height{};
  std::uint32_t enable_fxaa = 1;
  std::array<std::uint32_t, 3> reserved{};
};

static_assert(sizeof(tone_mapping_constants) == 32);

struct tone_mapping_graph_pass_desc {
  render_graph::resource_id hdr_color = render_graph::invalid_resource_id;
  render_graph::resource_id output = render_graph::invalid_resource_id;
  granit::texture_format output_format = granit::texture_format::undefined;
  tone_mapping_desc tone_mapping{};
};

using tone_mapping_record_callback =
    std::function<granit_result(render_graph::pass_context&, const tone_mapping_constants&)>;

/** 创建首版 PBR HDR 瞬态 Attachment 描述。 */
[[nodiscard]] granit_texture_desc make_hdr_attachment_desc(std::uint32_t width,
                                                           std::uint32_t height) noexcept;

/** 添加读取 HDR、写入最终颜色目标的 Tone Mapping Pass。 */
[[nodiscard]] render_graph::pass_id
add_tone_mapping_graph_pass(render_graph::serial_graph& graph, tone_mapping_graph_pass_desc desc,
                            tone_mapping_record_callback callback,
                            std::string name = "Tone Mapping");

} // namespace granit::lighting

#endif
