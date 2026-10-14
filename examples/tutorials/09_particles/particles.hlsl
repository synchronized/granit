// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
};

struct particle_instance {
  float4 position_scale;
  float4 color;
  float4 shape_scale_rotation;
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
  const float angle = particle.shape_scale_rotation.w;
  const float sine = sin(angle);
  const float cosine = cos(angle);
  const float3 scaled = input.position * particle.shape_scale_rotation.xyz;
  const float3 rotated = float3(cosine * scaled.x + sine * scaled.z, scaled.y,
                                -sine * scaled.x + cosine * scaled.z);
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
