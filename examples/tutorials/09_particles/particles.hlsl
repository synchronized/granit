// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct particle {
  float4 position_size;
  float4 color;
};

struct vertex_output {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

StructuredBuffer<particle> particles : register(t0, space0);

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const uint particle_id = vertex_id / 6;
  const uint corner_id = vertex_id % 6;
  const float2 corners[6] = {
      float2(-1.0, -1.0), float2(1.0, -1.0), float2(1.0, 1.0),
      float2(-1.0, -1.0), float2(1.0, 1.0), float2(-1.0, 1.0)};
  const particle value = particles[particle_id];
  vertex_output output;
  output.position = float4(value.position_size.xy + corners[corner_id] * value.position_size.z, 0.0, 1.0);
  output.color = value.color;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float distance_to_center = length(input.position.xy - floor(input.position.xy));
  return float4(input.color.rgb, input.color.a * saturate(1.0 - distance_to_center * 1.4));
}
