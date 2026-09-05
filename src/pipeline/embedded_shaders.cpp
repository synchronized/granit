// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/embedded_shaders.h"
#include "assets/shader_asset.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace granit::pipeline::detail {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t tone_mapping_vertex_bytes[]{
#include "granit_pipeline_tone_mapping.vert.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t tone_mapping_fragment_bytes[]{
#include "granit_pipeline_tone_mapping.frag.inc"
};

constexpr std::string_view tone_mapping_wgsl_source = R"(
struct ToneMappingConstants {
  exposure_scale: f32,
  encode_srgb: u32,
  inverse_width: f32,
  inverse_height: f32,
  enable_fxaa: u32,
  reserved_0: u32,
  reserved_1: u32,
  reserved_2: u32,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) uv: vec2<f32>,
};

@group(0) @binding(0) var<uniform> tone: ToneMappingConstants;
@group(0) @binding(1) var hdr_color: texture_2d<f32>;
@group(0) @binding(2) var hdr_sampler: sampler;

@vertex
fn vertex_main(@builtin(vertex_index) vertex_id: u32) -> VertexOutput {
  var positions = array<vec2<f32>, 3>(
      vec2<f32>(-1.0, -1.0), vec2<f32>(3.0, -1.0), vec2<f32>(-1.0, 3.0));
  var output: VertexOutput;
  output.position = vec4<f32>(positions[vertex_id], 0.5, 1.0);
  output.uv = positions[vertex_id] * vec2<f32>(0.5, -0.5) + vec2<f32>(0.5);
  return output;
}

fn aces_fitted(input: vec3<f32>) -> vec3<f32> {
  let color = max(input, vec3<f32>(0.0));
  return clamp(color * (2.51 * color + vec3<f32>(0.03)) /
                   (color * (2.43 * color + vec3<f32>(0.59)) + vec3<f32>(0.14)),
               vec3<f32>(0.0), vec3<f32>(1.0));
}

fn linear_to_srgb(input: vec3<f32>) -> vec3<f32> {
  let color = clamp(input, vec3<f32>(0.0), vec3<f32>(1.0));
  let low = color * 12.92;
  let high = 1.055 * pow(color, vec3<f32>(1.0 / 2.4)) - vec3<f32>(0.055);
  return select(high, low, color <= vec3<f32>(0.0031308));
}

@fragment
fn fragment_main(input: VertexOutput) -> @location(0) vec4<f32> {
  let texel = vec2<f32>(tone.inverse_width, tone.inverse_height);
  let center = textureSample(hdr_color, hdr_sampler, input.uv).rgb;
  let north = textureSample(hdr_color, hdr_sampler, input.uv + vec2<f32>(0.0, -texel.y)).rgb;
  let south = textureSample(hdr_color, hdr_sampler, input.uv + vec2<f32>(0.0, texel.y)).rgb;
  let west = textureSample(hdr_color, hdr_sampler, input.uv + vec2<f32>(-texel.x, 0.0)).rgb;
  let east = textureSample(hdr_color, hdr_sampler, input.uv + vec2<f32>(texel.x, 0.0)).rgb;
  let luma = vec3<f32>(0.299, 0.587, 0.114);
  let center_luma = dot(center, luma);
  let minimum_luma = min(center_luma, min(min(dot(north, luma), dot(south, luma)),
                                           min(dot(west, luma), dot(east, luma))));
  let maximum_luma = max(center_luma, max(max(dot(north, luma), dot(south, luma)),
                                           max(dot(west, luma), dot(east, luma))));
  let contrast = maximum_luma - minimum_luma;
  let filtered = (north + south + west + east) * 0.25;
  let threshold = max(0.0312, maximum_luma * 0.125);
  let blend = 0.5 * clamp((contrast - threshold) / max(contrast, 0.0001), 0.0, 1.0);
  let antialiased = select(center, mix(center, filtered, blend), tone.enable_fxaa != 0u);
  var color = aces_fitted(antialiased * tone.exposure_scale);
  if (tone.encode_srgb != 0u) {
    color = linear_to_srgb(color);
  }
  return vec4<f32>(color, 1.0);
}
)";

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

alignas(std::uint32_t) constexpr std::uint8_t canvas_vertex_manifest[]{
#include "unlit_canvas.vert.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_vertex_spirv[]{
#include "unlit_canvas.vert.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_vertex_wgsl[]{
#include "unlit_canvas.vert.grshader.wgsl.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_fragment_manifest[]{
#include "unlit_canvas.frag.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_fragment_spirv[]{
#include "unlit_canvas.frag.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_fragment_wgsl[]{
#include "unlit_canvas.frag.grshader.wgsl.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_srgb_manifest[]{
#include "unlit_canvas_encode_srgb.frag.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_srgb_spirv[]{
#include "unlit_canvas_encode_srgb.frag.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_srgb_wgsl[]{
#include "unlit_canvas_encode_srgb.frag.grshader.wgsl.inc"
};

struct embedded_shader_asset {
  std::span<const std::uint8_t> manifest;
  std::span<const std::uint8_t> spirv;
  std::span<const std::uint8_t> wgsl;
};

constexpr std::array canvas_shader_assets{
    embedded_shader_asset{canvas_vertex_manifest, canvas_vertex_spirv, canvas_vertex_wgsl},
    embedded_shader_asset{canvas_fragment_manifest, canvas_fragment_spirv, canvas_fragment_wgsl},
    embedded_shader_asset{canvas_srgb_manifest, canvas_srgb_spirv, canvas_srgb_wgsl}};

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

std::string_view tone_mapping_wgsl() noexcept { return tone_mapping_wgsl_source; }

std::span<const std::byte> shadow_depth_vertex_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(shadow_depth_vertex_bytes),
          sizeof(shadow_depth_vertex_bytes)};
}

std::span<const std::byte> shadow_depth_fragment_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(shadow_depth_fragment_bytes),
          sizeof(shadow_depth_fragment_bytes)};
}

std::string_view shadow_depth_vertex_wgsl() noexcept { return shadow_depth_vertex_wgsl_source; }

std::string_view shadow_depth_fragment_wgsl() noexcept { return shadow_depth_fragment_wgsl_source; }

std::span<const std::byte> canvas_material_package() noexcept {
  return {reinterpret_cast<const std::byte*>(canvas_material_bytes), sizeof(canvas_material_bytes)};
}

granit_result resolve_canvas_shader(void*, const std::uint8_t asset_id[32],
                                    granit_renderer_backend backend, std::uint32_t profile,
                                    granit_shader_asset_desc* asset) noexcept {
  if (asset_id == nullptr || asset == nullptr || profile != GRANIT_SHADER_PROFILE_PORTABLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (const auto& candidate : canvas_shader_assets) {
    granit::tools::shader_asset_view view;
    const auto manifest = std::as_bytes(candidate.manifest);
    if (granit::tools::decode_shader_asset(manifest, view) !=
            granit::tools::shader_asset_error::success ||
        std::memcmp(view.content_id.data(), asset_id, view.content_id.size()) != 0) {
      continue;
    }
    const auto sidecar = backend == GRANIT_RENDERER_BACKEND_VULKAN
                             ? candidate.spirv
                             : backend == GRANIT_RENDERER_BACKEND_WEBGPU ? candidate.wgsl
                                                                         : std::span<const std::uint8_t>{};
    if (sidecar.empty())
      return GRANIT_ERROR_UNSUPPORTED;
    *asset = GRANIT_SHADER_ASSET_DESC_INIT;
    asset->manifest_data = candidate.manifest.data();
    asset->manifest_size = candidate.manifest.size();
    asset->sidecar_data = sidecar.data();
    asset->sidecar_size = sidecar.size();
    return GRANIT_SUCCESS;
  }
  return GRANIT_ERROR_NOT_READY;
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
