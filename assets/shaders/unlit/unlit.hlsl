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

[[vk::binding(0, 1)]] cbuffer MaterialConstants {
  float4 base_color;
  float alpha_cutoff;
  float3 reserved_values;
};

#if GRANIT_UNLIT_TEXTURED
[[vk::binding(1, 1)]] Texture2D<float4> base_color_texture;
[[vk::binding(2, 1)]] SamplerState unlit_sampler;
#endif

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {float2(0.0, -0.65), float2(0.65, 0.65),
                               float2(-0.65, 0.65)};
  vertex_output output;
  output.position = float4(positions[vertex_id], 0.5, 1.0);
  output.uv = positions[vertex_id] * 0.5 + 0.5;
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
