// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct DrawData {
  translation: vec4f,
  color: vec4f,
};

@group(0) @binding(0) var<uniform> draw_data: DrawData;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) color: vec4f,
};

@vertex
fn main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
  var positions = array<vec2f, 3>(
    vec2f(-0.22, -0.30),
    vec2f(0.22, -0.30),
    vec2f(0.0, 0.30),
  );
  var output: VertexOutput;
  output.position = vec4f(positions[vertex_index] + draw_data.translation.xy, 0.0, 1.0);
  output.color = draw_data.color;
  return output;
}
