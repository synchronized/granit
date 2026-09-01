// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/embedded_shaders.h"

#include <cstdint>

namespace granit::pipeline::detail {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t tone_mapping_vertex_bytes[]{
#include "granit_pipeline_tone_mapping.vert.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t tone_mapping_fragment_bytes[]{
#include "granit_pipeline_tone_mapping.frag.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t shadow_depth_vertex_bytes[]{
#include "granit_pipeline_shadow_depth.vert.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t shadow_depth_fragment_bytes[]{
#include "granit_pipeline_shadow_depth.frag.inc"
};

constexpr std::string_view shadow_depth_vertex_wgsl_source = R"(
struct ObjectConstants {
  model: mat4x4<f32>,
  normal_matrix: mat4x4<f32>,
  object_id: vec4<u32>,
};

struct ShadowConstants {
  light_view_projection: mat4x4<f32>,
  shadow_depth_bias: f32,
  shadow_normal_bias: f32,
  shadow_texel_size: vec2<f32>,
};

@group(2) @binding(0) var<uniform> object: ObjectConstants;
@group(3) @binding(0) var<uniform> shadow: ShadowConstants;

@vertex
fn vertex_main(@location(0) position: vec3<f32>) -> @builtin(position) vec4<f32> {
  return shadow.light_view_projection * object.model * vec4<f32>(position, 1.0);
}
)";

constexpr std::string_view shadow_depth_fragment_wgsl_source = R"(
@fragment
fn fragment_main() {}
)";

alignas(std::uint32_t) constexpr std::uint8_t canvas_material_bytes[]{
#include "granit_pipeline_canvas.grmat.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t debug_world_vertex_bytes[]{
#include "granit_pipeline_debug_world.vert.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t debug_world_fragment_bytes[]{
#include "granit_pipeline_debug_world.frag.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t debug_world_srgb_fragment_bytes[]{
#include "granit_pipeline_debug_world_srgb.frag.inc"
};

} // namespace

std::span<const std::byte> tone_mapping_vertex_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(tone_mapping_vertex_bytes),
          sizeof(tone_mapping_vertex_bytes)};
}

std::span<const std::byte> tone_mapping_fragment_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(tone_mapping_fragment_bytes),
          sizeof(tone_mapping_fragment_bytes)};
}

std::span<const std::byte> shadow_depth_vertex_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(shadow_depth_vertex_bytes),
          sizeof(shadow_depth_vertex_bytes)};
}

std::span<const std::byte> shadow_depth_fragment_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(shadow_depth_fragment_bytes),
          sizeof(shadow_depth_fragment_bytes)};
}

std::string_view shadow_depth_vertex_wgsl() noexcept { return shadow_depth_vertex_wgsl_source; }

std::string_view shadow_depth_fragment_wgsl() noexcept {
  return shadow_depth_fragment_wgsl_source;
}

std::span<const std::byte> canvas_material_package() noexcept {
  return {reinterpret_cast<const std::byte*>(canvas_material_bytes), sizeof(canvas_material_bytes)};
}

std::span<const std::byte> debug_world_vertex_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(debug_world_vertex_bytes),
          sizeof(debug_world_vertex_bytes)};
}

std::span<const std::byte> debug_world_fragment_shader(bool encode_srgb) noexcept {
  if (encode_srgb) {
    return {reinterpret_cast<const std::byte*>(debug_world_srgb_fragment_bytes),
            sizeof(debug_world_srgb_fragment_bytes)};
  }
  return {reinterpret_cast<const std::byte*>(debug_world_fragment_bytes),
          sizeof(debug_world_fragment_bytes)};
}

} // namespace granit::pipeline::detail
