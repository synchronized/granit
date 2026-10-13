// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 texture_coordinate : TEXCOORD0;
};

struct vertex_output {
  float4 position : SV_Position;
  float2 texture_coordinate : TEXCOORD0;
};

[[vk::binding(0, 0)]] cbuffer FrameConstants {
  column_major float4x4 view_projection;
  float4 camera_position;
  float4 direction_to_light;
  float4 light_radiance;
};

[[vk::binding(0, 1)]] cbuffer MaterialConstants {
  float4 base_color;
  float metallic;
  float perceptual_roughness;
  float normal_scale;
  float occlusion_strength;
  float3 emissive;
  uint debug_display;
  float alpha_cutoff;
  uint uv1_mask;
  uint2 material_reserved;
};

[[vk::binding(0, 2)]] cbuffer ObjectConstants {
  column_major float4x4 model;
  column_major float4x4 normal_matrix;
  uint object_id;
  float transform_handedness;
  uint2 object_reserved;
};

[[vk::binding(0, 4)]] Texture2D<float4> resource_table_textures[];
[[vk::binding(1, 4)]] SamplerState resource_table_samplers[];

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  output.position = mul(view_projection, mul(model, float4(input.position, 1.0)));
  output.texture_coordinate = input.texture_coordinate;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float4 sampled = resource_table_textures[0].Sample(resource_table_samplers[0],
                                                            input.texture_coordinate);
  return base_color * sampled;
}
