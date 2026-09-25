// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct metaballs_uniforms {
  float2 resolution;
  float time;
  float padding;
};

[[vk::binding(0, 0)]] ConstantBuffer<metaballs_uniforms> scene;

float4 vertex_main(uint vertex_id : SV_VertexID) : SV_Position {
  const float2 positions[3] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
  return float4(positions[vertex_id], 0.5, 1.0);
}

float smooth_union(float first, float second, float radius) {
  const float blend = saturate(0.5 + 0.5 * (second - first) / radius);
  return lerp(second, first, blend) - radius * blend * (1.0 - blend);
}

float scene_distance(float3 sample_position) {
  const float time = scene.time;
  const float3 centers[5] = {
      float3(0.0, 0.15, 0.0),
      float3(sin(time * 0.9) * 0.85, cos(time * 0.7) * 0.45, 0.05),
      float3(cos(time * 0.65 + 1.2) * 0.75, sin(time * 0.8 + 0.4) * 0.55, -0.15),
      float3(sin(time * 0.55 + 2.4) * 0.65, -0.35, cos(time * 0.6) * 0.35),
      float3(-0.5, sin(time * 0.75 + 1.7) * 0.55, cos(time * 0.5 + 0.8) * 0.3)};

  float distance = length(sample_position - centers[0]) - 0.62;
  [unroll]
  for (uint index = 1; index < 5; ++index) {
    const float sphere = length(sample_position - centers[index]) - 0.42;
    distance = smooth_union(distance, sphere, 0.38);
  }
  return distance;
}

float3 estimate_normal(float3 sample_position) {
  const float epsilon = 0.001;
  const float2 offset = float2(epsilon, 0.0);
  return normalize(
      float3(scene_distance(sample_position + offset.xyy) -
                 scene_distance(sample_position - offset.xyy),
             scene_distance(sample_position + offset.yxy) -
                 scene_distance(sample_position - offset.yxy),
             scene_distance(sample_position + offset.yyx) -
                 scene_distance(sample_position - offset.yyx)));
}

float4 fragment_main(float4 position : SV_Position) : SV_Target0 {
  float2 uv = (position.xy * 2.0 - scene.resolution) / scene.resolution.y;
  uv.y = -uv.y;
  const float3 ray_origin = float3(0.0, 0.0, 3.6);
  const float3 ray_direction = normalize(float3(uv, -1.9));

  float travel = 0.0;
  bool hit = false;
  [loop]
  for (uint step = 0; step < 96; ++step) {
    const float distance = scene_distance(ray_origin + ray_direction * travel);
    if (distance < 0.001) {
      hit = true;
      break;
    }
    travel += distance;
    if (travel > 20.0)
      break;
  }

  const float3 background = lerp(float3(0.015, 0.025, 0.07), float3(0.10, 0.03, 0.16),
                                 saturate(uv.y * 0.4 + 0.55));
  if (!hit)
    return float4(background, 1.0);

  const float3 sample_position = ray_origin + ray_direction * travel;
  const float3 normal = estimate_normal(sample_position);
  const float3 light_direction = normalize(float3(-0.4, 0.75, 0.5));
  const float diffuse = saturate(dot(normal, light_direction));
  const float fresnel = pow(1.0 - saturate(dot(normal, -ray_direction)), 2.5);
  const float height_mix = saturate(sample_position.y * 0.65 + 0.5);
  const float3 base = lerp(float3(0.15, 0.45, 0.95), float3(0.95, 0.2, 0.55), height_mix);
  const float3 color = base * (0.18 + 0.82 * diffuse) + fresnel * float3(0.35, 0.8, 0.95);
  return float4(color, 1.0);
}
