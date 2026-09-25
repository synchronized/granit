// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
  float time;
  float3 padding;
};

[[vk::binding(0, 0)]] ConstantBuffer<scene_uniforms> scene;

struct vertex_input {
  float3 position : POSITION;
  float4 offset_scale : TEXCOORD0;
  float4 color : COLOR0;
};

struct vertex_output {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

vertex_output vertex_main(vertex_input input) {
  const float angle = scene.time + input.offset_scale.x * 0.17 + input.offset_scale.y * 0.11;
  const float sine = sin(angle);
  const float cosine = cos(angle);
  const float3 local = input.position * input.offset_scale.w;
  const float3 rotated = float3(cosine * local.x + sine * local.z, local.y,
                                -sine * local.x + cosine * local.z);

  vertex_output output;
  output.position = mul(scene.view_projection,
                        float4(rotated + input.offset_scale.xyz, 1.0));
  output.color = input.color;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  return input.color;
}
