// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
  float4 light_direction;
  float4 ambient;
  float4 debug;
};

struct particle_instance {
  float4 position_scale;
  float4 color;
  float4 shape_scale;
  float4 rotation;
};

[[vk::binding(0, 0)]] ConstantBuffer<scene_uniforms> scene;
StructuredBuffer<particle_instance> particles : register(t1, space0);

struct vertex_input {
  float3 position : POSITION;
  float3 normal : NORMAL;
};

struct vertex_output {
  float4 position : SV_Position;
  float4 color : COLOR0;
  float3 normal : NORMAL;
};

float3 rotate_vector(float3 value, float3 rotation) {
  const float3 cx = float3(1.0, cos(rotation.x), sin(rotation.x));
  const float3 cy = float3(cos(rotation.y), 1.0, sin(rotation.y));
  const float3 cz = float3(cos(rotation.z), sin(rotation.z), 1.0);
  float3 rotated = float3(value.x, cx.y * value.y - cx.z * value.z,
                          cx.z * value.y + cx.y * value.z);
  rotated = float3(cy.x * rotated.x + cy.z * rotated.z, rotated.y,
                   -cy.z * rotated.x + cy.x * rotated.z);
  return float3(cz.x * rotated.x - cz.y * rotated.y,
                cz.y * rotated.x + cz.x * rotated.y, rotated.z);
}

vertex_output vertex_main(vertex_input input, uint instance_id : SV_InstanceID) {
  const particle_instance particle = particles[instance_id];
  const float3 scaled = input.position * particle.shape_scale.xyz;
  const float3 rotated = rotate_vector(scaled, particle.rotation.xyz);
  const float3 world_position = particle.position_scale.xyz +
                                rotated * particle.position_scale.w;
  vertex_output output;
  output.position = mul(scene.view_projection, float4(world_position, 1.0));
  output.color = particle.color;
  output.normal = rotate_vector(input.normal, particle.rotation.xyz);
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float3 normal = normalize(input.normal);
  if (scene.debug.x > 0.5)
    return float4(normal * 0.5 + 0.5, 1.0);
  const float diffuse = max(dot(normal, normalize(scene.light_direction.xyz)), 0.0);
  const float lighting = scene.ambient.x + diffuse * (1.0 - scene.ambient.x);
  return float4(input.color.rgb * lighting, input.color.a);
}
