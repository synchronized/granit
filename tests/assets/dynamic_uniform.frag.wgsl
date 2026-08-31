// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

@group(0) @binding(1) var base_color_texture: texture_2d<f32>;
@group(0) @binding(2) var base_color_sampler: sampler;

@fragment
fn main(@location(0) color: vec4f, @location(1) uv: vec2f) -> @location(0) vec4f {
  return textureSample(base_color_texture, base_color_sampler, uv) * color;
}
