// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 texture_coordinate : TEXCOORD0;
#if GRANIT_RESOURCE_TABLE_VARIANT == 2 || GRANIT_RESOURCE_TABLE_VARIANT == 4
  [[vk::location(4)]] float2 texture_coordinate_1 : TEXCOORD1;
#endif
#if GRANIT_RESOURCE_TABLE_VARIANT == 3 || GRANIT_RESOURCE_TABLE_VARIANT == 4
  [[vk::location(5)]] float4 color : COLOR0;
#endif
};

struct vertex_output {
  float4 position : SV_Position;
  float2 texture_coordinate : TEXCOORD0;
#if GRANIT_RESOURCE_TABLE_VARIANT == 2 || GRANIT_RESOURCE_TABLE_VARIANT == 4 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 12 || GRANIT_RESOURCE_TABLE_VARIANT == 14 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 16 || GRANIT_RESOURCE_TABLE_VARIANT == 18 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 20 || GRANIT_RESOURCE_TABLE_VARIANT == 22
  float2 texture_coordinate_1 : TEXCOORD1;
#endif
#if GRANIT_RESOURCE_TABLE_VARIANT == 3 || GRANIT_RESOURCE_TABLE_VARIANT == 4 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 13 || GRANIT_RESOURCE_TABLE_VARIANT == 14 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 17 || GRANIT_RESOURCE_TABLE_VARIANT == 18 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 21 || GRANIT_RESOURCE_TABLE_VARIANT == 22
  float4 color : TEXCOORD2;
#endif
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
#if GRANIT_RESOURCE_TABLE_VARIANT == 2 || GRANIT_RESOURCE_TABLE_VARIANT == 4
  output.texture_coordinate_1 = input.texture_coordinate_1;
#endif
#if GRANIT_RESOURCE_TABLE_VARIANT == 3 || GRANIT_RESOURCE_TABLE_VARIANT == 4
  output.color = input.color;
#endif
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
#if GRANIT_RESOURCE_TABLE_VARIANT == 12 || GRANIT_RESOURCE_TABLE_VARIANT == 14 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 16 || GRANIT_RESOURCE_TABLE_VARIANT == 18 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 20 || GRANIT_RESOURCE_TABLE_VARIANT == 22
  const float2 resolved_texture_coordinate =
      (uv1_mask != 0) ? input.texture_coordinate_1 : input.texture_coordinate;
#else
  const float2 resolved_texture_coordinate = input.texture_coordinate;
#endif
  const float4 sampled_base_color = resource_table_textures[base_color_texture_index].Sample(
      resource_table_samplers[base_color_sampler_index], resolved_texture_coordinate);
  const float4 sampled_metallic_roughness =
      resource_table_textures[metallic_roughness_texture_index].Sample(
          resource_table_samplers[metallic_roughness_sampler_index], resolved_texture_coordinate);
  const float3 sampled_normal =
      resource_table_textures[normal_texture_index].Sample(
          resource_table_samplers[normal_sampler_index], resolved_texture_coordinate).xyz;
  const float4 sampled_occlusion = resource_table_textures[occlusion_texture_index].Sample(
      resource_table_samplers[occlusion_sampler_index], resolved_texture_coordinate);
  const float4 sampled_emissive = resource_table_textures[emissive_texture_index].Sample(
      resource_table_samplers[emissive_sampler_index], resolved_texture_coordinate);
  const float normal_influence = 0.5 + 0.5 * sampled_normal.z;
  const float lighting_influence =
      0.75 + 0.25 * sampled_metallic_roughness.g * sampled_occlusion.r;
  float4 resolved_base_color = base_color * sampled_base_color;
#if GRANIT_RESOURCE_TABLE_VARIANT == 13 || GRANIT_RESOURCE_TABLE_VARIANT == 14 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 17 || GRANIT_RESOURCE_TABLE_VARIANT == 18 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 21 || GRANIT_RESOURCE_TABLE_VARIANT == 22
  resolved_base_color *= input.color;
#endif
#if GRANIT_RESOURCE_TABLE_VARIANT == 15 || GRANIT_RESOURCE_TABLE_VARIANT == 16 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 17 || GRANIT_RESOURCE_TABLE_VARIANT == 18
  clip(resolved_base_color.a - alpha_cutoff);
#elif GRANIT_RESOURCE_TABLE_VARIANT == 19 || GRANIT_RESOURCE_TABLE_VARIANT == 20 || \
    GRANIT_RESOURCE_TABLE_VARIANT == 21 || GRANIT_RESOURCE_TABLE_VARIANT == 22
  resolved_base_color.rgb *= resolved_base_color.a;
#endif
  const float3 color = resolved_base_color.rgb * normal_influence * lighting_influence +
                       sampled_emissive.rgb * 0.1;
  return float4(color, resolved_base_color.a);
}
