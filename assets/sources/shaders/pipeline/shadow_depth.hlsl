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

struct mask_vertex_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(3)]] float2 texture_coordinate : TEXCOORD0;
};

struct mask_vertex_output {
  float4 position : SV_Position;
  float2 texture_coordinate : TEXCOORD0;
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
  uint3 material_reserved;
};

[[vk::binding(1, 1)]] Texture2D<float4> base_color_texture;
[[vk::binding(6, 1)]] SamplerState pbr_sampler;

mask_vertex_output mask_vertex_main(mask_vertex_input input) {
  mask_vertex_output output;
  output.position = mul(light_view_projection, mul(model, float4(input.position, 1.0)));
  output.texture_coordinate = input.texture_coordinate;
  return output;
}

void mask_fragment_main(mask_vertex_output input) {
  const float alpha =
      base_color.a * base_color_texture.Sample(pbr_sampler, input.texture_coordinate).a;
  clip(alpha - alpha_cutoff);
}
