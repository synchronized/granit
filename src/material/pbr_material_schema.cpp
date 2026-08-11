// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_material_schema.h"

#include <array>

namespace granit::material {

pbr_vertex_layout_error
validate_pbr_vertex_layout(std::span<const material_vertex_buffer_layout> vertex_buffers,
                           pbr_texture_flags textures) noexcept {
  if ((textures & ~pbr_texture_all) != 0)
    return pbr_vertex_layout_error::invalid_texture_flags;

  std::array<bool, 4> locations{};
  for (const auto& buffer : vertex_buffers) {
    for (const auto& attribute : buffer.attributes) {
      if (attribute.location < locations.size())
        locations[attribute.location] = true;
    }
  }
  if (!locations[pbr_vertex_location_position])
    return pbr_vertex_layout_error::missing_position;
  if (!locations[pbr_vertex_location_normal])
    return pbr_vertex_layout_error::missing_normal;
  if (textures != 0 && !locations[pbr_vertex_location_uv0])
    return pbr_vertex_layout_error::missing_uv0;
  if ((textures & pbr_texture_normal) != 0 && !locations[pbr_vertex_location_tangent])
    return pbr_vertex_layout_error::missing_tangent;
  return pbr_vertex_layout_error::none;
}

} // namespace granit::material
