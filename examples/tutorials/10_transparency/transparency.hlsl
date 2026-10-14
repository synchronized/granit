// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms { float4x4 view_projection; };
ConstantBuffer<scene_uniforms> scene : register(b0, space0);
struct input_data { float3 position : POSITION; float4 transform : TEXCOORD0; float4 color_mode : TEXCOORD1; };
struct output_data { float4 position : SV_Position; float4 color : COLOR0; float mode : TEXCOORD0; };

output_data vertex_main(input_data input) {
  output_data output;
  output.position = mul(scene.view_projection, float4(input.position * input.transform.w + input.transform.xyz, 1.0));
  output.color = float4(input.color_mode.rgb, input.color_mode.a == 2.0 ? 0.45 : 1.0);
  output.mode = input.color_mode.a;
  return output;
}

float4 fragment_main(output_data input) : SV_Target0 {
  if (input.mode == 1.0 && frac((input.position.x + input.position.y) * 0.08) < 0.35) discard;
  return input.color;
}
