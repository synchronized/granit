// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_material_schema.h"

#include <array>

namespace granit::material {

pbr_shader_texture_class classify_pbr_shader_textures(pbr_texture_flags textures) noexcept {
  if ((textures & ~pbr_texture_all) != 0)
    return pbr_shader_texture_class::invalid;
  if ((textures & pbr_texture_normal) != 0)
    return pbr_shader_texture_class::normal_mapped;
  if (textures != 0)
    return pbr_shader_texture_class::textured;
  return pbr_shader_texture_class::untextured;
}

pbr_vertex_layout_error
validate_pbr_vertex_layout(std::span<const material_vertex_buffer_layout> vertex_buffers,
                           pbr_texture_flags textures) noexcept {
  const auto shader_class = classify_pbr_shader_textures(textures);
  if (shader_class == pbr_shader_texture_class::invalid)
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
  if (shader_class != pbr_shader_texture_class::untextured && !locations[pbr_vertex_location_uv0])
    return pbr_vertex_layout_error::missing_uv0;
  if (shader_class == pbr_shader_texture_class::normal_mapped &&
      !locations[pbr_vertex_location_tangent])
    return pbr_vertex_layout_error::missing_tangent;
  return pbr_vertex_layout_error::none;
}

} // namespace granit::material
