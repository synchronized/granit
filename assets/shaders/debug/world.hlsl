// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_DEBUG_ENCODE_SRGB
#define GRANIT_DEBUG_ENCODE_SRGB 0
#endif

struct vertex_input {
  float4 clip_position : POSITION;
  uint color : COLOR0;
};

struct vertex_output {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  output.position = input.clip_position;
  output.color = float4(input.color & 0xff, (input.color >> 8) & 0xff,
                        (input.color >> 16) & 0xff, (input.color >> 24) & 0xff) /
                 255.0;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  float4 color = input.color;
#if GRANIT_DEBUG_ENCODE_SRGB
  const float3 low = color.rgb * 12.92;
  const float3 high = 1.055 * pow(max(color.rgb, 0.0), 1.0 / 2.4) - 0.055;
  color.rgb = lerp(high, low, color.rgb <= 0.0031308);
#endif
  return color;
}
