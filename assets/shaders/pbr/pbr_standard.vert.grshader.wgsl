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
  @location(0) world_position: vec3f,
  @location(1) world_normal: vec3f,
  @location(2) world_tangent: vec4f,
  @location(3) texture_coordinate: vec2f,
  @location(4) vertex_normal: vec3f,
  @location(5) vertex_tangent: vec3f,
}

@group(0) @binding(0) var<uniform> frame: FrameConstants;
@group(2) @binding(0) var<uniform> object: ObjectConstants;

@vertex
fn vertex_main(
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec4f,
    @location(3) texture_coordinate: vec2f) -> VertexOutput {
  let world_position = object.model * vec4f(position, 1.0);
  var output: VertexOutput;
  output.position = frame.view_projection * world_position;
  output.world_position = world_position.xyz;
  output.world_normal = normalize((object.normal_matrix * vec4f(normal, 0.0)).xyz);
  output.world_tangent = vec4f(
      normalize((object.model * vec4f(tangent.xyz, 0.0)).xyz), tangent.w);
  output.texture_coordinate = texture_coordinate;
  output.vertex_normal = normal;
  output.vertex_tangent = tangent.xyz;
  return output;
}
