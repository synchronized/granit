// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  let positions = array<vec2f, 3>(
    vec2f(0.0, -0.7),
    vec2f(0.7, 0.7),
    vec2f(-0.7, 0.7),
  );
  return vec4f(positions[index], 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
  return vec4f(0.2, 0.7, 0.4, 1.0);
}
