// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
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
};

struct vertex_output {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

vertex_output vertex_main(vertex_input input, uint instance_id : SV_InstanceID) {
  const particle_instance particle = particles[instance_id];
  const float3 scaled = input.position * particle.shape_scale.xyz;
  const float3 cx = float3(1.0, cos(particle.rotation.x), sin(particle.rotation.x));
  const float3 cy = float3(cos(particle.rotation.y), 1.0, sin(particle.rotation.y));
  const float3 cz = float3(cos(particle.rotation.z), sin(particle.rotation.z), 1.0);
  float3 rotated = float3(scaled.x, cx.y * scaled.y - cx.z * scaled.z,
                          cx.z * scaled.y + cx.y * scaled.z);
  rotated = float3(cy.x * rotated.x + cy.z * rotated.z, rotated.y,
                   -cy.z * rotated.x + cy.x * rotated.z);
  rotated = float3(cz.x * rotated.x - cz.y * rotated.y,
                   cz.y * rotated.x + cz.x * rotated.y, rotated.z);
  const float3 world_position = particle.position_scale.xyz +
                                rotated * particle.position_scale.w;
  vertex_output output;
  output.position = mul(scene.view_projection, float4(world_position, 1.0));
  output.color = particle.color;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  return input.color;
}
