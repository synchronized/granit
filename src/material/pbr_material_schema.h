// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_MATERIAL_SCHEMA_H
#define GRANIT_MATERIAL_PBR_MATERIAL_SCHEMA_H

#include "material/material_package.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace granit::material {

using pbr_texture_flags = std::uint32_t;
inline constexpr std::string_view pbr_texture_feature_name = "pbr_texture_mask";
inline constexpr pbr_texture_flags pbr_texture_base_color = UINT32_C(1) << 0;
inline constexpr pbr_texture_flags pbr_texture_metallic_roughness = UINT32_C(1) << 1;
inline constexpr pbr_texture_flags pbr_texture_normal = UINT32_C(1) << 2;
inline constexpr pbr_texture_flags pbr_texture_occlusion = UINT32_C(1) << 3;
inline constexpr pbr_texture_flags pbr_texture_emissive = UINT32_C(1) << 4;
inline constexpr pbr_texture_flags pbr_texture_all =
    pbr_texture_base_color | pbr_texture_metallic_roughness | pbr_texture_normal |
    pbr_texture_occlusion | pbr_texture_emissive;

inline constexpr std::uint32_t pbr_binding_constants = 0;
inline constexpr std::uint32_t pbr_binding_base_color = 1;
inline constexpr std::uint32_t pbr_binding_metallic_roughness = 2;
inline constexpr std::uint32_t pbr_binding_normal = 3;
inline constexpr std::uint32_t pbr_binding_occlusion = 4;
inline constexpr std::uint32_t pbr_binding_emissive = 5;
inline constexpr std::uint32_t pbr_binding_sampler = 6;
inline constexpr std::array<std::string_view, 5> pbr_texture_parameter_names{
    "base_color_texture", "metallic_roughness_texture", "normal_texture", "occlusion_texture",
    "emissive_texture"};
inline constexpr std::string_view pbr_sampler_parameter_name = "pbr_sampler";

inline constexpr std::uint32_t pbr_vertex_location_position = 0;
inline constexpr std::uint32_t pbr_vertex_location_normal = 1;
inline constexpr std::uint32_t pbr_vertex_location_tangent = 2;
inline constexpr std::uint32_t pbr_vertex_location_uv0 = 3;

enum class pbr_vertex_layout_error : std::uint8_t {
  none,
  invalid_texture_flags,
  missing_position,
  missing_normal,
  missing_uv0,
  missing_tangent,
};

/** 按 H-03 标准 location 检查网格是否满足指定 PBR 纹理变体。 */
[[nodiscard]] pbr_vertex_layout_error
validate_pbr_vertex_layout(std::span<const material_vertex_buffer_layout> vertex_buffers,
                           pbr_texture_flags textures) noexcept;

} // namespace granit::material

#endif
