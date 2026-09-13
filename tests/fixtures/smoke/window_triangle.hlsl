// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_input {
  float2 position : TEXCOORD0;
  float3 color : TEXCOORD1;
};

struct vertex_output {
  float4 position : SV_Position;
  float3 color : TEXCOORD0;
};

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  output.position = float4(input.position, 0.0f, 1.0f);
  output.color = input.color;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  return float4(input.color, 1.0f);
}
