// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

static const float PI = 3.14159265358979323846;
static const float MINIMUM_PERCEPTUAL_ROUGHNESS = 0.045;

struct vertex_output {
  float4 position : SV_Position;
  float3 normal : TEXCOORD0;
  float4 tangent : TEXCOORD1;
  float2 uv : TEXCOORD2;
};

[[vk::binding(0, 1)]] cbuffer MaterialConstants {
  float4 base_color;
  float metallic;
  float perceptual_roughness;
  float normal_scale;
  float occlusion_strength;
  float3 emissive;
  float reserved_value;
};

[[vk::binding(1, 1)]] Texture2D<float4> base_color_texture;
[[vk::binding(2, 1)]] Texture2D<float4> metallic_roughness_texture;
[[vk::binding(3, 1)]] Texture2D<float4> normal_texture;
[[vk::binding(4, 1)]] Texture2D<float4> occlusion_texture;
[[vk::binding(5, 1)]] Texture2D<float4> emissive_texture;
[[vk::binding(6, 1)]] SamplerState pbr_sampler;

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {float2(0.0, -0.65), float2(0.65, 0.65),
                               float2(-0.65, 0.65)};
  vertex_output output;
  output.position = float4(positions[vertex_id], 0.5, 1.0);
  output.normal = float3(0.0, 0.0, 1.0);
  output.tangent = float4(1.0, 0.0, 0.0, 1.0);
  output.uv = positions[vertex_id] * 0.5 + 0.5;
  return output;
}

float3 fresnel_schlick(float view_dot_half, float3 reflectance_at_normal) {
  const float factor = pow(1.0 - saturate(view_dot_half), 5.0);
  return reflectance_at_normal + (1.0 - reflectance_at_normal) * factor;
}

float distribution_ggx(float normal_dot_half, float perceptual_roughness) {
  const float roughness = max(saturate(perceptual_roughness), MINIMUM_PERCEPTUAL_ROUGHNESS);
  const float alpha = roughness * roughness;
  const float alpha_squared = alpha * alpha;
  const float denominator =
      normal_dot_half * normal_dot_half * (alpha_squared - 1.0) + 1.0;
  return alpha_squared / (PI * denominator * denominator);
}

float visibility_smith_correlated(float normal_dot_view, float normal_dot_light,
                                  float perceptual_roughness) {
  const float roughness = max(saturate(perceptual_roughness), MINIMUM_PERCEPTUAL_ROUGHNESS);
  const float alpha = roughness * roughness;
  const float alpha_squared = alpha * alpha;
  const float lambda_view = normal_dot_light *
                            sqrt(normal_dot_view * normal_dot_view * (1.0 - alpha_squared) +
                                 alpha_squared);
  const float lambda_light = normal_dot_view *
                             sqrt(normal_dot_light * normal_dot_light * (1.0 - alpha_squared) +
                                  alpha_squared);
  return 0.5 / (lambda_view + lambda_light);
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float4 sampled_base_color = base_color_texture.Sample(pbr_sampler, input.uv);
  const float4 sampled_metallic_roughness =
      metallic_roughness_texture.Sample(pbr_sampler, input.uv);
  const float3 sampled_normal = normal_texture.Sample(pbr_sampler, input.uv).xyz * 2.0 - 1.0;
  const float sampled_occlusion = occlusion_texture.Sample(pbr_sampler, input.uv).r;
  const float3 sampled_emissive = emissive_texture.Sample(pbr_sampler, input.uv).rgb;
  const float3 geometric_normal = normalize(input.normal);
  const float3 tangent = normalize(input.tangent.xyz);
  const float3 bitangent = normalize(cross(geometric_normal, tangent)) * input.tangent.w;
  const float3 normal = normalize(tangent * sampled_normal.x * normal_scale +
                                  bitangent * sampled_normal.y * normal_scale +
                                  geometric_normal * sampled_normal.z);
  const float4 resolved_base_color = base_color * sampled_base_color;
  const float resolved_metallic = metallic * sampled_metallic_roughness.b;
  const float resolved_roughness = perceptual_roughness * sampled_metallic_roughness.g;
  const float3 view = float3(0.0, 0.0, 1.0);
  const float3 light = float3(0.0, 0.0, 1.0);
  const float3 half_vector = normalize(view + light);
  const float normal_dot_view = saturate(dot(normal, view));
  const float normal_dot_light = saturate(dot(normal, light));
  const float clamped_metallic = saturate(resolved_metallic);
  const float3 reflectance = lerp(0.04.xxx, resolved_base_color.rgb, clamped_metallic);
  const float3 fresnel = fresnel_schlick(saturate(dot(view, half_vector)), reflectance);
  const float distribution =
      distribution_ggx(saturate(dot(normal, half_vector)), resolved_roughness);
  const float visibility = visibility_smith_correlated(
      normal_dot_view, normal_dot_light, resolved_roughness);
  const float3 diffuse =
      resolved_base_color.rgb * (1.0 - fresnel) * (1.0 - clamped_metallic) / PI;
  // 首版尚无 IBL，遮蔽值暂时调制总光照；H-05 接入间接光后只作用于间接项。
  const float occlusion = lerp(1.0, sampled_occlusion, saturate(occlusion_strength));
  const float3 direct =
      (diffuse + fresnel * distribution * visibility) * normal_dot_light * occlusion;
  return float4(direct + max(emissive, 0.0.xxx) * sampled_emissive,
                resolved_base_color.a);
}
