// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

static const float PI = 3.14159265358979323846;
static const float MINIMUM_PERCEPTUAL_ROUGHNESS = 0.045;

struct vertex_output {
  float4 position : SV_Position;
  float3 normal : TEXCOORD0;
};

vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {float2(0.0, -0.65), float2(0.65, 0.65),
                               float2(-0.65, 0.65)};
  vertex_output output;
  output.position = float4(positions[vertex_id], 0.5, 1.0);
  output.normal = float3(0.0, 0.0, 1.0);
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
  // H-03C 先使用固定无纹理参数，H-03D 再接入材质常量与纹理。
  const float3 base_color = float3(0.8, 0.2, 0.1);
  const float metallic = 0.5;
  const float perceptual_roughness = 0.5;
  const float3 normal = normalize(input.normal);
  const float3 view = float3(0.0, 0.0, 1.0);
  const float3 light = float3(0.0, 0.0, 1.0);
  const float3 half_vector = normalize(view + light);
  const float normal_dot_view = saturate(dot(normal, view));
  const float normal_dot_light = saturate(dot(normal, light));
  const float3 reflectance = lerp(0.04.xxx, base_color, metallic);
  const float3 fresnel = fresnel_schlick(saturate(dot(view, half_vector)), reflectance);
  const float distribution =
      distribution_ggx(saturate(dot(normal, half_vector)), perceptual_roughness);
  const float visibility = visibility_smith_correlated(
      normal_dot_view, normal_dot_light, perceptual_roughness);
  const float3 diffuse =
      base_color * (1.0 - fresnel) * (1.0 - metallic) / PI;
  const float3 color = (diffuse + fresnel * distribution * visibility) * normal_dot_light;
  return float4(color, 1.0);
}
