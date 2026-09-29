// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/pbr_material.h>

#include "material/pbr_material_schema.h"

#include <array>
#include <cstdint>

extern "C" granit_pbr_vertex_layout_result
granit_pbr_validate_vertex_layout(const granit_vertex_buffer_layout* vertex_buffers,
                                  uint32_t vertex_buffer_count, granit_pbr_texture_flags textures,
                                  granit_pbr_texture_flags uv1_mask) {
  if ((textures & ~GRANIT_PBR_TEXTURE_ALL) != 0)
    return GRANIT_PBR_VERTEX_LAYOUT_INVALID_TEXTURE_FLAGS;
  if (vertex_buffers == nullptr && vertex_buffer_count != 0)
    return GRANIT_PBR_VERTEX_LAYOUT_INVALID_ARGUMENT;

  if ((uv1_mask & ~textures) != 0)
    return GRANIT_PBR_VERTEX_LAYOUT_INVALID_UV1_MASK;
  std::array<bool, 5> locations{};
  for (std::uint32_t buffer_index = 0; buffer_index < vertex_buffer_count; ++buffer_index) {
    const auto& buffer = vertex_buffers[buffer_index];
    if (buffer.reserved != 0 || (buffer.attributes == nullptr && buffer.attribute_count != 0))
      return GRANIT_PBR_VERTEX_LAYOUT_INVALID_ARGUMENT;
    for (std::uint32_t attribute_index = 0; attribute_index < buffer.attribute_count;
         ++attribute_index) {
      const auto& attribute = buffer.attributes[attribute_index];
      if (attribute.reserved != 0)
        return GRANIT_PBR_VERTEX_LAYOUT_INVALID_ARGUMENT;
      if (attribute.location < locations.size())
        locations[attribute.location] = true;
    }
  }
  if (!locations[GRANIT_PBR_VERTEX_LOCATION_POSITION])
    return GRANIT_PBR_VERTEX_LAYOUT_MISSING_POSITION;
  if (!locations[GRANIT_PBR_VERTEX_LOCATION_NORMAL])
    return GRANIT_PBR_VERTEX_LAYOUT_MISSING_NORMAL;
  if (textures != 0 && !locations[GRANIT_PBR_VERTEX_LOCATION_UV0])
    return GRANIT_PBR_VERTEX_LAYOUT_MISSING_UV0;
  if (uv1_mask != 0 && !locations[GRANIT_PBR_VERTEX_LOCATION_UV1])
    return GRANIT_PBR_VERTEX_LAYOUT_MISSING_UV1;
  if ((textures & GRANIT_PBR_TEXTURE_NORMAL) != 0 && !locations[GRANIT_PBR_VERTEX_LOCATION_TANGENT])
    return GRANIT_PBR_VERTEX_LAYOUT_MISSING_TANGENT;
  return GRANIT_PBR_VERTEX_LAYOUT_VALID;
}

extern "C" uint64_t granit_pbr_material_variant_key(granit_pbr_texture_flags textures,
                                                    granit_pbr_alpha_mode alpha_mode,
                                                    uint32_t double_sided, uint32_t has_uv1) {
  if (granit::material::classify_pbr_shader_textures(textures) ==
          granit::material::pbr_shader_texture_class::invalid ||
      alpha_mode > GRANIT_PBR_ALPHA_MODE_BLEND || double_sided > 1 || has_uv1 > 1)
    return 0;
  return granit::material::standard_pbr_variant_key(textures, alpha_mode, double_sided != 0, false,
                                                    has_uv1 != 0);
}
