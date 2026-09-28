// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#define MAXIMUM_LIGHT_COUNT 64

struct geometry_uniforms {
  float4x4 view;
  float4x4 view_projection;
  float4 animation;
};

struct point_light {
  float4 position_radius;
  float4 color_intensity;
};

struct lighting_uniforms {
  float2 resolution;
  float exposure;
  uint output_mode;
  float ambient;
  float time;
  uint light_count;
  float padding;
  point_light lights[MAXIMUM_LIGHT_COUNT];
};

[[vk::binding(0, 0)]] ConstantBuffer<geometry_uniforms> geometry_scene;
[[vk::binding(0, 0)]] ConstantBuffer<lighting_uniforms> lighting_scene;
[[vk::binding(1, 0)]] Texture2D<float4> position_depth_texture;
[[vk::binding(2, 0)]] Texture2D<float4> normal_texture;
[[vk::binding(3, 0)]] Texture2D<float4> albedo_material_texture;
[[vk::binding(4, 0)]] SamplerState gbuffer_sampler;

struct geometry_vertex_input {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float4 offset_scale : TEXCOORD0;
  float4 color : COLOR0;
};

struct geometry_vertex_output {
  float4 position : SV_Position;
  float3 view_position : TEXCOORD0;
  float3 view_normal : TEXCOORD1;
  float4 color : COLOR0;
};

struct gbuffer_output {
  float4 position_depth : SV_Target0;
  float4 normal : SV_Target1;
  float4 albedo_material : SV_Target2;
};

geometry_vertex_output geometry_vertex_main(geometry_vertex_input input) {
  const float angle =
      geometry_scene.animation.x + input.offset_scale.x * 0.17 + input.offset_scale.y * 0.11;
  const float sine = sin(angle);
  const float cosine = cos(angle);
  const float3 local = input.position * input.offset_scale.w;
  const float3 rotated = float3(cosine * local.x + sine * local.z, local.y,
                                -sine * local.x + cosine * local.z);
  const float3 world_position = rotated + input.offset_scale.xyz;
  const float3 local_normal = input.normal;
  const float3 world_normal = normalize(float3(cosine * local_normal.x + sine * local_normal.z,
                                               local_normal.y,
                                               -sine * local_normal.x + cosine * local_normal.z));

  geometry_vertex_output output;
  output.position = mul(geometry_scene.view_projection, float4(world_position, 1.0));
  output.view_position = mul(geometry_scene.view, float4(world_position, 1.0)).xyz;
  output.view_normal = normalize(mul((float3x3)geometry_scene.view, world_normal));
  output.color = input.color;
  return output;
}

gbuffer_output geometry_fragment_main(geometry_vertex_output input) {
  gbuffer_output output;
  output.position_depth = float4(input.view_position, max(-input.view_position.z, 0.0));
  output.normal = float4(normalize(input.view_normal), 1.0);
  output.albedo_material = float4(input.color.rgb, 0.45);
  return output;
}

struct lighting_vertex_output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

lighting_vertex_output lighting_vertex_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
  lighting_vertex_output output;
  output.position = float4(positions[vertex_id], 0.5, 1.0);
  output.uv = positions[vertex_id] * float2(0.5, -0.5) + 0.5;
  return output;
}

float4 lighting_fragment_main(lighting_vertex_output input) : SV_Target0 {
  const float2 uv = saturate(input.uv);
  const float4 position_depth = position_depth_texture.SampleLevel(gbuffer_sampler, uv, 0.0);
  const float3 normal = normalize(normal_texture.SampleLevel(gbuffer_sampler, uv, 0.0).xyz);
  const float4 albedo_material = albedo_material_texture.SampleLevel(gbuffer_sampler, uv, 0.0);

  if (lighting_scene.output_mode == 1)
    return float4(position_depth.xyz * 0.08 + 0.5, 1.0);
  if (lighting_scene.output_mode == 2)
    return float4(normal * 0.5 + 0.5, 1.0);
  if (lighting_scene.output_mode == 3)
    return float4(albedo_material.rgb, 1.0);

  if (position_depth.w <= 0.0001)
    return float4(0.018, 0.025, 0.05, 1.0);

  float3 color = albedo_material.rgb * lighting_scene.ambient;
  [loop]
  for (uint index = 0; index < lighting_scene.light_count; ++index) {
    const point_light light = lighting_scene.lights[index];
    const float3 to_light = light.position_radius.xyz - position_depth.xyz;
    const float distance = length(to_light);
    const float3 direction = to_light / max(distance, 0.0001);
    const float attenuation = saturate(1.0 - distance / light.position_radius.w);
    const float diffuse = saturate(dot(normal, direction));
    color += albedo_material.rgb * light.color_intensity.rgb * light.color_intensity.w * diffuse *
             attenuation * attenuation;
  }
  color = 1.0 - exp(-color * lighting_scene.exposure);
  return float4(color, 1.0);
}
