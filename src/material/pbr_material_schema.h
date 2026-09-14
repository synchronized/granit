// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_MATERIAL_SCHEMA_H
#define GRANIT_MATERIAL_PBR_MATERIAL_SCHEMA_H

#include "asset_formats/material/material_package.h"

#include <granit/pipeline/pbr_material.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace granit::material {

using pbr_texture_flags = std::uint32_t;
inline constexpr std::string_view pbr_texture_feature_name = GRANIT_PBR_TEXTURE_FEATURE_NAME;
inline constexpr pbr_texture_flags pbr_texture_base_color = GRANIT_PBR_TEXTURE_BASE_COLOR;
inline constexpr pbr_texture_flags pbr_texture_metallic_roughness =
    GRANIT_PBR_TEXTURE_METALLIC_ROUGHNESS;
inline constexpr pbr_texture_flags pbr_texture_normal = GRANIT_PBR_TEXTURE_NORMAL;
inline constexpr pbr_texture_flags pbr_texture_occlusion = GRANIT_PBR_TEXTURE_OCCLUSION;
inline constexpr pbr_texture_flags pbr_texture_emissive = GRANIT_PBR_TEXTURE_EMISSIVE;
inline constexpr pbr_texture_flags pbr_texture_all = GRANIT_PBR_TEXTURE_ALL;

inline constexpr std::uint32_t pbr_binding_constants = GRANIT_PBR_BINDING_CONSTANTS;
inline constexpr std::uint32_t pbr_binding_base_color = GRANIT_PBR_BINDING_BASE_COLOR;
inline constexpr std::uint32_t pbr_binding_metallic_roughness =
    GRANIT_PBR_BINDING_METALLIC_ROUGHNESS;
inline constexpr std::uint32_t pbr_binding_normal = GRANIT_PBR_BINDING_NORMAL;
inline constexpr std::uint32_t pbr_binding_occlusion = GRANIT_PBR_BINDING_OCCLUSION;
inline constexpr std::uint32_t pbr_binding_emissive = GRANIT_PBR_BINDING_EMISSIVE;
inline constexpr std::uint32_t pbr_binding_sampler = GRANIT_PBR_BINDING_SAMPLER;
inline constexpr std::array<std::string_view, 5> pbr_texture_parameter_names{
    GRANIT_PBR_PARAMETER_BASE_COLOR_TEXTURE, GRANIT_PBR_PARAMETER_METALLIC_ROUGHNESS_TEXTURE,
    GRANIT_PBR_PARAMETER_NORMAL_TEXTURE, GRANIT_PBR_PARAMETER_OCCLUSION_TEXTURE,
    GRANIT_PBR_PARAMETER_EMISSIVE_TEXTURE};
inline constexpr std::string_view pbr_sampler_parameter_name = GRANIT_PBR_PARAMETER_SAMPLER;

inline constexpr std::uint32_t pbr_vertex_location_position = GRANIT_PBR_VERTEX_LOCATION_POSITION;
inline constexpr std::uint32_t pbr_vertex_location_normal = GRANIT_PBR_VERTEX_LOCATION_NORMAL;
inline constexpr std::uint32_t pbr_vertex_location_tangent = GRANIT_PBR_VERTEX_LOCATION_TANGENT;
inline constexpr std::uint32_t pbr_vertex_location_uv0 = GRANIT_PBR_VERTEX_LOCATION_UV0;

enum class pbr_vertex_layout_error : std::uint8_t {
  none,
  invalid_texture_flags,
  missing_position,
  missing_normal,
  missing_uv0,
  missing_tangent,
};

/** 控制 Shader 资源布局的 PBR 纹理类别；同类的精确纹理掩码共享二进制。 */
enum class pbr_shader_texture_class : std::uint8_t {
  untextured,
  textured,
  normal_mapped,
  invalid,
};

/** 将材质的精确纹理 feature 掩码归并为少量 Shader 结构类别。 */
[[nodiscard]] pbr_shader_texture_class
classify_pbr_shader_textures(pbr_texture_flags textures) noexcept;

/** 按 H-03 标准 location 检查网格是否满足指定 PBR 纹理变体。 */
[[nodiscard]] pbr_vertex_layout_error
validate_pbr_vertex_layout(std::span<const material_vertex_buffer_layout> vertex_buffers,
                           pbr_texture_flags textures) noexcept;

} // namespace granit::material

#endif
