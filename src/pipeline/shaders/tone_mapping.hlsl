// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct vertex_output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]] cbuffer ToneMappingConstants {
  float exposure_scale;
  uint encode_srgb;
  float inverse_width;
  float inverse_height;
  uint enable_fxaa;
  uint3 reserved;
};
[[vk::binding(1, 0)]] Texture2D<float4> hdr_color;
[[vk::binding(2, 0)]] SamplerState hdr_sampler;

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
  vertex_output output;
  output.position = float4(positions[vertex_id], 0.5, 1.0);
  output.uv = positions[vertex_id] * float2(0.5, -0.5) + 0.5;
  return output;
}

float3 aces_fitted(float3 color) {
  color = max(color, 0.0.xxx);
  return saturate(color * (2.51 * color + 0.03) /
                  (color * (2.43 * color + 0.59) + 0.14));
}

float3 linear_to_srgb(float3 color) {
  color = saturate(color);
  const float3 low = color * 12.92;
  const float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
  return lerp(high, low, color <= 0.0031308.xxx);
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float2 texel = float2(inverse_width, inverse_height);
  const float3 center = hdr_color.Sample(hdr_sampler, input.uv).rgb;
  const float3 north = hdr_color.Sample(hdr_sampler, input.uv + float2(0.0, -texel.y)).rgb;
  const float3 south = hdr_color.Sample(hdr_sampler, input.uv + float2(0.0, texel.y)).rgb;
  const float3 west = hdr_color.Sample(hdr_sampler, input.uv + float2(-texel.x, 0.0)).rgb;
  const float3 east = hdr_color.Sample(hdr_sampler, input.uv + float2(texel.x, 0.0)).rgb;
  const float3 luma = float3(0.299, 0.587, 0.114);
  const float center_luma = dot(center, luma);
  const float minimum_luma = min(center_luma, min(min(dot(north, luma), dot(south, luma)),
                                                   min(dot(west, luma), dot(east, luma))));
  const float maximum_luma = max(center_luma, max(max(dot(north, luma), dot(south, luma)),
                                                   max(dot(west, luma), dot(east, luma))));
  const float contrast = maximum_luma - minimum_luma;
  const float3 filtered = (north + south + west + east) * 0.25;
  const float threshold = max(0.0312, maximum_luma * 0.125);
  const float blend = 0.5 * saturate((contrast - threshold) / max(contrast, 0.0001));
  const float3 antialiased = enable_fxaa != 0 ? lerp(center, filtered, blend) : center;
  float3 color = aces_fitted(antialiased * exposure_scale);
  if (encode_srgb != 0)
    color = linear_to_srgb(color);
  return float4(color, 1.0);
}
