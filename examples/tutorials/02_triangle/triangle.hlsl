// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_output {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {float2(-0.65, -0.65), float2(0.65, -0.65),
                               float2(0.0, 0.65)};
  const float4 colors[3] = {float4(1.0, 0.2, 0.2, 1.0), float4(0.2, 1.0, 0.2, 1.0),
                            float4(0.2, 0.4, 1.0, 1.0)};
  vertex_output output;
  output.position = float4(positions[vertex_id], 0.0, 1.0);
  output.color = colors[vertex_id];
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  return input.color;
}
