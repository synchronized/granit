// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct fragment_input {
  float2 uv : TEXCOORD0;
};

[[vk::binding(0, 1)]] cbuffer MaterialConstants {
  float4 base_color;
  float roughness;
  float metallic;
  float2 reserved_value;
};

[[vk::binding(1, 1)]] Texture2D<float4> albedo_texture;
[[vk::binding(2, 1)]] SamplerState linear_sampler;

float4 fragment_main(fragment_input input) : SV_Target0 {
  return albedo_texture.Sample(linear_sampler, input.uv) * base_color;
}
