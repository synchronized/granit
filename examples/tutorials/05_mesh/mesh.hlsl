// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct mesh_uniforms {
  float4x4 model_view_projection;
};

[[vk::binding(0, 0)]] ConstantBuffer<mesh_uniforms> camera;
[[vk::binding(1, 0)]] Texture2D<float4> checker_texture;
[[vk::binding(2, 0)]] SamplerState checker_sampler;

struct vertex_input {
  float3 position : POSITION;
  float2 uv : TEXCOORD0;
};

struct vertex_output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  output.position = mul(camera.model_view_projection, float4(input.position, 1.0));
  output.uv = input.uv;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  return checker_texture.Sample(checker_sampler, input.uv);
}
