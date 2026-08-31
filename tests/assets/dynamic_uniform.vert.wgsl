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
  @location(1) uv: vec2f,
};

@vertex
fn main(@location(0) position: vec2f, @location(1) uv: vec2f) -> VertexOutput {
  var output: VertexOutput;
  output.position = vec4f(position + draw_data.translation.xy, 0.0, 1.0);
  output.color = draw_data.color;
  output.uv = uv;
  return output;
}
