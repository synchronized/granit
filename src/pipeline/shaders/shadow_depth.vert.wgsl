// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct ObjectConstants {
  model: mat4x4<f32>,
  normal_matrix: mat4x4<f32>,
  object_id: vec4<u32>,
};

struct ShadowConstants {
  light_view_projection: mat4x4<f32>,
  shadow_depth_bias: f32,
  shadow_normal_bias: f32,
  shadow_texel_size: vec2<f32>,
};

@group(2) @binding(0) var<uniform> object: ObjectConstants;
@group(3) @binding(0) var<uniform> shadow: ShadowConstants;

@vertex
fn vertex_main(@location(0) position: vec3<f32>) -> @builtin(position) vec4<f32> {
  return shadow.light_view_projection * object.model * vec4<f32>(position, 1.0);
}
