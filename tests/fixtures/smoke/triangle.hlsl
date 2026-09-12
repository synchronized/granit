// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_output {
  float4 position : SV_Position;
  float3 color : TEXCOORD0;
};

vertex_output vertex_main(uint vertex_index : SV_VertexID) {
  static const float2 positions[3] = {
      float2(0.0f, -0.6f), float2(0.6f, 0.6f), float2(-0.6f, 0.6f)};
  static const float3 colors[3] = {
      float3(1.0f, 0.15f, 0.1f), float3(0.1f, 1.0f, 0.2f), float3(0.15f, 0.3f, 1.0f)};
  vertex_output output;
  output.position = float4(positions[vertex_index], 0.0f, 1.0f);
  output.color = colors[vertex_index];
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  return float4(input.color, 1.0f);
}
