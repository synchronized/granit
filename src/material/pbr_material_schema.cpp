// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_material_schema.h"
#include "asset_formats/material/material_package.h"

#include <algorithm>
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

std::uint64_t standard_pbr_variant_key(pbr_texture_flags textures, std::uint32_t alpha_mode,
                                       bool double_sided, bool reflected, bool has_uv1,
                                       bool has_vertex_color) noexcept {
  std::array<material_feature_value, 6> features{};
  std::size_t count = 0;
  features[count++] = {make_feature_id(pbr_texture_feature_name), textures};
  if (alpha_mode != GRANIT_PBR_ALPHA_MODE_OPAQUE)
    features[count++] = {make_feature_id(pbr_alpha_mode_feature_name), alpha_mode};
  if (double_sided)
    features[count++] = {make_feature_id(pbr_double_sided_feature_name), 1};
  if (reflected)
    features[count++] = {make_feature_id(pbr_transform_reflected_feature_name), 1};
  if (has_uv1)
    features[count++] = {make_feature_id(pbr_uv1_feature_name), 1};
  if (has_vertex_color)
    features[count++] = {make_feature_id(pbr_vertex_color_feature_name), 1};
  auto selected = std::span{features}.first(count);
  std::ranges::sort(selected, {}, &material_feature_value::id);
  return make_variant_key(selected);
}

pbr_vertex_layout_error
validate_pbr_vertex_layout(std::span<const material_vertex_buffer_layout> vertex_buffers,
                           pbr_texture_flags textures, pbr_texture_flags uv1_mask,
                           bool has_vertex_color) noexcept {
  const auto shader_class = classify_pbr_shader_textures(textures);
  if (shader_class == pbr_shader_texture_class::invalid)
    return pbr_vertex_layout_error::invalid_texture_flags;

  if ((uv1_mask & ~textures) != 0)
    return pbr_vertex_layout_error::invalid_uv1_mask;
  std::array<bool, 6> locations{};
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
  if (uv1_mask != 0 && !locations[pbr_vertex_location_uv1])
    return pbr_vertex_layout_error::missing_uv1;
  if (has_vertex_color && !locations[pbr_vertex_location_color])
    return pbr_vertex_layout_error::missing_color;
  if (shader_class == pbr_shader_texture_class::normal_mapped &&
      !locations[pbr_vertex_location_tangent])
    return pbr_vertex_layout_error::missing_tangent;
  return pbr_vertex_layout_error::none;
}

} // namespace granit::material
