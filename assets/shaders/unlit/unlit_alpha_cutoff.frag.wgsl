// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct MaterialConstants {
  base_color: vec4f,
  alpha_cutoff: f32,
  reserved_values: vec3f,
}

@group(1) @binding(0) var<uniform> material: MaterialConstants;

@fragment
fn fragment_main() -> @location(0) vec4f {
  if (material.base_color.a < material.alpha_cutoff) {
    discard;
  }
  return material.base_color;
}
