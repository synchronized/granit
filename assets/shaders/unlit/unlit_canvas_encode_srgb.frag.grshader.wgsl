// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct MaterialConstants {
  base_color: vec4f,
  alpha_cutoff: f32,
  reserved_values: vec3f,
}

@group(1) @binding(0) var<uniform> material: MaterialConstants;

fn encode_srgb(linear: vec3f) -> vec3f {
  let low = linear * 12.92;
  let high = 1.055 * pow(max(linear, vec3f(0.0)), vec3f(1.0 / 2.4)) - vec3f(0.055);
  return select(high, low, linear <= vec3f(0.0031308));
}

@fragment
fn fragment_main(@location(1) input_color: vec4f) -> @location(0) vec4f {
  let color = material.base_color * input_color;
  return vec4f(encode_srgb(color.rgb), color.a);
}
