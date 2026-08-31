// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

@group(0) @binding(1) var base_color_texture: texture_2d<f32>;
@group(0) @binding(2) var base_color_sampler: sampler;
@group(0) @binding(3) var normal_texture: texture_2d<f32>;
@group(0) @binding(4) var metallic_roughness_texture: texture_2d<f32>;

@fragment
fn main(@location(0) color: vec4f, @location(1) uv: vec2f,
        @location(2) normal: vec3f) -> @location(0) vec4f {
  let sampled_normal = textureSample(normal_texture, base_color_sampler, uv).xyz * 2.0 - 1.0;
  let metallic_roughness =
      textureSample(metallic_roughness_texture, base_color_sampler, uv).bg;
  let material_scale = 0.5 + 0.25 * max(dot(normalize(normal), sampled_normal), 0.0) +
      0.25 * metallic_roughness.x;
  return textureSample(base_color_texture, base_color_sampler, uv) * color * material_scale;
}
