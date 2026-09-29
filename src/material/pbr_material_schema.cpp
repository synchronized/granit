// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_material_schema.h"
#include "asset_formats/material/material_package.h"

#include <array>
#include <utility>

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

std::uint64_t standard_pbr_variant_key(pbr_texture_flags textures, bool reflected) noexcept {
  std::array<material_feature_value, 2> features{
      material_feature_value{make_feature_id(pbr_texture_feature_name), textures},
      material_feature_value{make_feature_id(pbr_transform_reflected_feature_name), 1}};
  if (!reflected)
    return make_variant_key(std::span{features}.first(1));
  if (features[1].id < features[0].id)
    std::swap(features[0], features[1]);
  return make_variant_key(features);
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
