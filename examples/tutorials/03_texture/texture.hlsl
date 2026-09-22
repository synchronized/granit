// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[6] = {
    float2(-0.7, -0.7), float2(0.7, -0.7), float2(0.7, 0.7),
    float2(-0.7, -0.7), float2(0.7, 0.7), float2(-0.7, 0.7)
  };
  const float2 uvs[6] = {
    float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, 0.0),
    float2(0.0, 1.0), float2(1.0, 0.0), float2(0.0, 0.0)
  };
  vertex_output output;
  output.position = float4(positions[vertex_id], 0.0, 1.0);
  output.uv = uvs[vertex_id];
  return output;
}

[[vk::binding(0, 0)]] Texture2D<float4> checker_texture;
[[vk::binding(1, 0)]] SamplerState checker_sampler;

float4 fragment_main(vertex_output input) : SV_Target0 {
  return checker_texture.Sample(checker_sampler, input.uv);
}
