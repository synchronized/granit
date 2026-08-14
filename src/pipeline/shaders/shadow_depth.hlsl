// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

[[vk::binding(0, 2)]] cbuffer ObjectConstants {
  column_major float4x4 model;
  column_major float4x4 normal_matrix;
  uint4 object_id;
};

[[vk::binding(0, 3)]] cbuffer ShadowConstants {
  column_major float4x4 light_view_projection;
  float shadow_depth_bias;
  float shadow_normal_bias;
  float2 shadow_texel_size;
};

float4 vertex_main(float3 position : POSITION) : SV_Position {
  return mul(light_view_projection, mul(model, float4(position, 1.0)));
}

void fragment_main() {}
