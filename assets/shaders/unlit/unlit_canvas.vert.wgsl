// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct FrameConstants {
  view_projection: mat4x4<f32>,
  camera_position: vec4f,
  direction_to_light: vec4f,
  light_radiance: vec4f,
}

struct ObjectConstants {
  model: mat4x4<f32>,
  normal_matrix: mat4x4<f32>,
  object_id: vec4u,
}

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) color: vec4f,
}

@group(0) @binding(0) var<uniform> frame: FrameConstants;
@group(2) @binding(0) var<uniform> object: ObjectConstants;

@vertex
fn canvas_vertex_main(@location(0) position: vec2f, @location(1) uv: vec2f,
                      @location(2) packed_color: u32) -> VertexOutput {
  let world_position = object.model * vec4f(position, 0.0, 1.0);
  var output: VertexOutput;
  output.position = frame.view_projection * world_position;
  output.uv = uv;
  output.color = vec4f(f32(packed_color & 0xffu), f32((packed_color >> 8u) & 0xffu),
                       f32((packed_color >> 16u) & 0xffu),
                       f32((packed_color >> 24u) & 0xffu)) /
                 255.0;
  return output;
}
