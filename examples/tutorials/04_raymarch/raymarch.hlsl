// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct raymarch_uniforms {
  float2 resolution;
  float time;
  float padding;
};

[[vk::binding(0, 0)]] ConstantBuffer<raymarch_uniforms> scene;

float4 vertex_main(uint vertex_id : SV_VertexID) : SV_Position {
  const float2 positions[3] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
  return float4(positions[vertex_id], 0.5, 1.0);
}

float scene_distance(float3 sample_position) {
  const float sphere = length(sample_position - float3(0.0, 0.15, 0.0)) - 0.85;
  const float box = length(max(abs(sample_position - float3(-1.15, -0.15, -0.45)) -
                               float3(0.42, 0.42, 0.42), 0.0)) - 0.08;
  const float ground = sample_position.y + 1.0;
  return min(min(sphere, box), ground);
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
  const float3 ray_origin = float3(0.0, 0.25, 3.5);
  const float3 ray_direction = normalize(float3(uv, -1.8));

  float travel = 0.0;
  bool hit = false;
  [loop]
  for (uint step = 0; step < 80; ++step) {
    const float distance = scene_distance(ray_origin + ray_direction * travel);
    if (distance < 0.001) {
      hit = true;
      break;
    }
    travel += distance;
    if (travel > 20.0)
      break;
  }

  const float3 background = lerp(float3(0.025, 0.04, 0.10), float3(0.16, 0.06, 0.20),
                                 saturate(uv.y * 0.35 + 0.55));
  if (!hit)
    return float4(background, 1.0);

  const float3 sample_position = ray_origin + ray_direction * travel;
  const float3 normal = estimate_normal(sample_position);
  const float3 light_direction = normalize(float3(-0.45, 0.8, 0.35));
  const float diffuse = saturate(dot(normal, light_direction));
  const float rim = pow(1.0 - saturate(dot(normal, -ray_direction)), 3.0);
  const float3 base = sample_position.x < -0.65 ? float3(0.25, 0.85, 0.95)
                                                : float3(0.95, 0.38, 0.18);
  const float3 color = base * (0.18 + 0.82 * diffuse) + rim * float3(0.35, 0.25, 0.8);
  return float4(color, 1.0);
}
