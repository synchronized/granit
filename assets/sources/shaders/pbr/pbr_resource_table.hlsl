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
  uint base_color_texture_index;
  uint base_color_sampler_index;
  uint metallic_roughness_texture_index;
  uint normal_texture_index;
  uint occlusion_texture_index;
  uint emissive_texture_index;
  uint metallic_roughness_sampler_index;
  uint normal_sampler_index;
  uint occlusion_sampler_index;
  uint emissive_sampler_index;
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
  const float4 sampled_base_color = resource_table_textures[base_color_texture_index].Sample(
      resource_table_samplers[base_color_sampler_index],
                                                            input.texture_coordinate);
  const float4 sampled_metallic_roughness =
      resource_table_textures[metallic_roughness_texture_index].Sample(
          resource_table_samplers[metallic_roughness_sampler_index], input.texture_coordinate);
  const float3 sampled_normal =
      resource_table_textures[normal_texture_index].Sample(
          resource_table_samplers[normal_sampler_index], input.texture_coordinate).xyz;
  const float4 sampled_occlusion = resource_table_textures[occlusion_texture_index].Sample(
      resource_table_samplers[occlusion_sampler_index], input.texture_coordinate);
  const float4 sampled_emissive = resource_table_textures[emissive_texture_index].Sample(
      resource_table_samplers[emissive_sampler_index], input.texture_coordinate);
  const float normal_influence = 0.5 + 0.5 * sampled_normal.z;
  const float lighting_influence =
      0.75 + 0.25 * sampled_metallic_roughness.g * sampled_occlusion.r;
  const float3 color = base_color.rgb * sampled_base_color.rgb * normal_influence *
                       lighting_influence + sampled_emissive.rgb * 0.1;
  return float4(color, base_color.a * sampled_base_color.a);
}
