// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_UNLIT_TEXTURED
#define GRANIT_UNLIT_TEXTURED 0
#endif

#ifndef GRANIT_UNLIT_VERTEX_COLOR
#define GRANIT_UNLIT_VERTEX_COLOR 0
#endif

#ifndef GRANIT_UNLIT_ALPHA_CUTOFF
#define GRANIT_UNLIT_ALPHA_CUTOFF 0
#endif

struct vertex_output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;
};

[[vk::binding(0, 0)]] cbuffer FrameConstants {
  column_major float4x4 view_projection;
  float4 camera_position;
  float4 direction_to_light;
  float4 light_radiance;
};

[[vk::binding(0, 1)]] cbuffer MaterialConstants {
  float4 base_color;
  float alpha_cutoff;
  float3 reserved_values;
};

#if GRANIT_UNLIT_TEXTURED
[[vk::binding(1, 1)]] Texture2D<float4> base_color_texture;
[[vk::binding(2, 1)]] SamplerState unlit_sampler;
#endif

[[vk::binding(0, 2)]] cbuffer ObjectConstants {
  column_major float4x4 model;
  column_major float4x4 normal_matrix;
  uint4 object_id;
};

struct vertex_input {
  float3 position : POSITION;
};

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  const float4 world_position = mul(model, float4(input.position, 1.0));
  output.position = mul(view_projection, world_position);
  output.uv = input.position.xy * 0.5 + 0.5;
  output.color = 1.0.xxxx;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  float4 color = base_color;
#if GRANIT_UNLIT_TEXTURED
  color *= base_color_texture.Sample(unlit_sampler, input.uv);
#endif
#if GRANIT_UNLIT_VERTEX_COLOR
  color *= input.color;
#endif
#if GRANIT_UNLIT_ALPHA_CUTOFF
  clip(color.a - alpha_cutoff);
#endif
  return color;
}
