// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  let position = vec2f(f32(index), 0.0);
  return vec4f(position, 0.0, );
}
