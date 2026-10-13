// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct fragment_input {
  float4 position : SV_Position;
};

[[vk::binding(0, 4)]] Texture2D<float4> resource_table_textures[2];
[[vk::binding(1, 4)]] SamplerState resource_table_samplers[2];

float4 main(fragment_input input) : SV_Target0 {
  return resource_table_textures[0].Sample(resource_table_samplers[0], float2(0.5, 0.5));
}
