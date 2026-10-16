// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
  float4 light_direction;
};

[[vk::binding(0, 0)]] ConstantBuffer<scene_uniforms> scene;

struct vertex_input {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;
};

struct vertex_output {
  float4 position : SV_Position;
  float3 normal : TEXCOORD0;
  float2 uv : TEXCOORD1;
  float4 color : COLOR0;
};

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  output.position = mul(scene.view_projection, float4(input.position, 1.0));
  output.normal = input.normal;
  output.uv = input.uv;
  output.color = input.color;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float diffuse = 0.35 + 0.65 * max(dot(normalize(input.normal),
                                                normalize(scene.light_direction.xyz)), 0.0);
  const float checker = fmod(floor(input.uv.x * 4.0) + floor(input.uv.y * 4.0), 2.0);
  const float3 tint = lerp(input.color.rgb, input.color.rgb * 0.65, checker);
  return float4(tint * diffuse, 1.0);
}
