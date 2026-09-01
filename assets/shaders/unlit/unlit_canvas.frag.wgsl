// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct MaterialConstants {
  base_color: vec4f,
  alpha_cutoff: f32,
  reserved_values: vec3f,
}

@group(1) @binding(0) var<uniform> material: MaterialConstants;

@fragment
fn fragment_main(@location(1) color: vec4f) -> @location(0) vec4f {
  return material.base_color * color;
}
