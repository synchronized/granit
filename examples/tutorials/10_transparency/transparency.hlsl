// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
  float4 light_direction;
  float4 ambient;
};
ConstantBuffer<scene_uniforms> scene : register(b0, space0);
struct input_data {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float4 transform : TEXCOORD0;
  float4 color_mode : TEXCOORD1;
};
struct output_data {
  float4 position : SV_Position;
  float4 color : COLOR0;
  float3 normal : NORMAL;
  float mode : TEXCOORD0;
};

output_data vertex_main(input_data input) {
  output_data output;
  output.position = mul(scene.view_projection, float4(input.position * input.transform.w + input.transform.xyz, 1.0));
  output.color = float4(input.color_mode.rgb, input.color_mode.a == 2.0 ? 0.45 : 1.0);
  output.normal = input.normal;
  output.mode = input.color_mode.a;
  return output;
}

float4 fragment_main(output_data input) : SV_Target0 {
  if (input.mode == 1.0 && frac((input.position.x + input.position.y) * 0.08) < 0.35) discard;
  const float diffuse = max(dot(normalize(input.normal), normalize(scene.light_direction.xyz)), 0.0);
  const float lighting = scene.ambient.x + diffuse * (1.0 - scene.ambient.x);
  return float4(input.color.rgb * lighting, input.color.a);
}
