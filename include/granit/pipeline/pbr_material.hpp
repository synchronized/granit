// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_PBR_MATERIAL_HPP_
#define GRANIT_PIPELINE_PBR_MATERIAL_HPP_

#include <granit/pipeline/pbr_material.h>
#include <granit/renderer/pipeline.hpp>

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace granit {

enum class pbr_texture : std::uint32_t {
  none = 0,
  base_color = GRANIT_PBR_TEXTURE_BASE_COLOR,
  metallic_roughness = GRANIT_PBR_TEXTURE_METALLIC_ROUGHNESS,
  normal = GRANIT_PBR_TEXTURE_NORMAL,
  occlusion = GRANIT_PBR_TEXTURE_OCCLUSION,
  emissive = GRANIT_PBR_TEXTURE_EMISSIVE,
  all = GRANIT_PBR_TEXTURE_ALL,
};

[[nodiscard]] constexpr pbr_texture operator|(pbr_texture left, pbr_texture right) noexcept {
  return static_cast<pbr_texture>(static_cast<std::uint32_t>(left) |
                                  static_cast<std::uint32_t>(right));
}

enum class pbr_vertex_layout_result : std::uint32_t {
  valid = GRANIT_PBR_VERTEX_LAYOUT_VALID,
  invalid_texture_flags = GRANIT_PBR_VERTEX_LAYOUT_INVALID_TEXTURE_FLAGS,
  missing_position = GRANIT_PBR_VERTEX_LAYOUT_MISSING_POSITION,
  missing_normal = GRANIT_PBR_VERTEX_LAYOUT_MISSING_NORMAL,
  missing_uv0 = GRANIT_PBR_VERTEX_LAYOUT_MISSING_UV0,
  missing_tangent = GRANIT_PBR_VERTEX_LAYOUT_MISSING_TANGENT,
  invalid_argument = GRANIT_PBR_VERTEX_LAYOUT_INVALID_ARGUMENT,
};

/** 检查公共顶点布局是否满足指定标准 PBR 纹理变体。 */
[[nodiscard]] inline pbr_vertex_layout_result
validate_pbr_vertex_layout(std::span<const granit_vertex_buffer_layout> vertex_buffers,
                           pbr_texture textures) noexcept {
  if (vertex_buffers.size() > std::numeric_limits<std::uint32_t>::max())
    return pbr_vertex_layout_result::invalid_argument;
  return static_cast<pbr_vertex_layout_result>(granit_pbr_validate_vertex_layout(
      vertex_buffers.data(), static_cast<std::uint32_t>(vertex_buffers.size()),
      static_cast<std::uint32_t>(textures)));
}

/** 检查强类型 C++ 顶点布局是否满足指定标准 PBR 纹理变体。 */
[[nodiscard]] inline pbr_vertex_layout_result
validate_pbr_vertex_layout(std::span<const vertex_buffer_layout> vertex_buffers,
                           pbr_texture textures) noexcept {
  if (vertex_buffers.size() > std::numeric_limits<std::uint32_t>::max())
    return pbr_vertex_layout_result::invalid_argument;
  try {
    std::vector<std::vector<granit_vertex_attribute>> attributes;
    std::vector<granit_vertex_buffer_layout> native;
    attributes.reserve(vertex_buffers.size());
    native.reserve(vertex_buffers.size());
    for (const auto& layout : vertex_buffers) {
      if (layout.attributes.size() > std::numeric_limits<std::uint32_t>::max())
        return pbr_vertex_layout_result::invalid_argument;
      auto& native_attributes = attributes.emplace_back();
      native_attributes.reserve(layout.attributes.size());
      for (const auto& attribute : layout.attributes) {
        native_attributes.push_back({.location = attribute.location,
                                     .format = static_cast<granit_vertex_format>(attribute.format),
                                     .offset = attribute.offset,
                                     .reserved = attribute.reserved});
      }
      native.push_back({.stride = layout.stride,
                        .step_mode = static_cast<granit_vertex_step_mode>(layout.step_mode),
                        .attribute_count = static_cast<std::uint32_t>(native_attributes.size()),
                        .reserved = 0,
                        .attributes = native_attributes.data()});
    }
    return validate_pbr_vertex_layout(native, textures);
  } catch (...) {
    return pbr_vertex_layout_result::invalid_argument;
  }
}

} // namespace granit

#endif
