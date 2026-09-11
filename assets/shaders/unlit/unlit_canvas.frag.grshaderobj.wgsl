// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct MaterialConstants {
  base_color: vec4f,
  alpha_cutoff: vec4f,
}

@group(1) @binding(0) var<uniform> material: MaterialConstants;
@group(1) @binding(1) var base_color_texture: texture_2d<f32>;
@group(1) @binding(2) var unlit_sampler: sampler;

@fragment
fn fragment_main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {
  return material.base_color * textureSample(base_color_texture, unlit_sampler, uv) * color;
}
