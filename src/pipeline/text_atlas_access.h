// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_ATLAS_ACCESS_H_
#define GRANIT_PIPELINE_TEXT_ATLAS_ACCESS_H_

#include <granit/pipeline/text_atlas.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

namespace granit::pipeline::detail {

struct text_atlas_glyph {
  granit_texture_view view = GRANIT_NULL_HANDLE;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  float u0 = 0;
  float v0 = 0;
  float u1 = 0;
  float v1 = 0;
  float width = 0;
  float height = 0;
  float bearing_x = 0;
  float bearing_y = 0;
};

[[nodiscard]] granit_result text_atlas_resolve_glyph(granit_renderer renderer,
                                                      granit_text_atlas atlas, uint64_t font_key,
                                                      uint32_t glyph_id,
                                                      text_atlas_glyph& glyph) noexcept;

} // namespace granit::pipeline::detail

#endif
