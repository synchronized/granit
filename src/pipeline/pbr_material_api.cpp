// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/pbr_material.h>

#include <array>

extern "C" granit_pbr_vertex_layout_result
granit_pbr_validate_vertex_layout(const granit_vertex_buffer_layout* vertex_buffers,
                                  uint32_t vertex_buffer_count, granit_pbr_texture_flags textures) {
  if ((textures & ~GRANIT_PBR_TEXTURE_ALL) != 0)
    return GRANIT_PBR_VERTEX_LAYOUT_INVALID_TEXTURE_FLAGS;
  if (vertex_buffers == nullptr && vertex_buffer_count != 0)
    return GRANIT_PBR_VERTEX_LAYOUT_INVALID_ARGUMENT;

  std::array<bool, 4> locations{};
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
  if ((textures & GRANIT_PBR_TEXTURE_NORMAL) != 0 && !locations[GRANIT_PBR_VERTEX_LOCATION_TANGENT])
    return GRANIT_PBR_VERTEX_LAYOUT_MISSING_TANGENT;
  return GRANIT_PBR_VERTEX_LAYOUT_VALID;
}
