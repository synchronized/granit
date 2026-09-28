// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct scene_uniforms {
  float4x4 view_projection;
  float4 grid_iso_time_capacity;
  float4 metaball_parameters;
};

struct generated_vertex {
  float4 position;
  float4 normal;
};

[[vk::binding(0, 0)]] ConstantBuffer<scene_uniforms> scene;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> field_samples;
[[vk::binding(2, 0)]] RWStructuredBuffer<int> triangle_table;
[[vk::binding(3, 0)]] RWStructuredBuffer<generated_vertex> generated_vertices;
[[vk::binding(4, 0)]] RWByteAddressBuffer generation_state;

uint sample_index(uint3 coordinate, uint grid_size) {
  return coordinate.x + grid_size * (coordinate.y + grid_size * coordinate.z);
}

float3 sample_position(uint3 coordinate, uint grid_size) {
  return (float3(coordinate) / float(grid_size - 1) - 0.5) * 3.0;
}

float density(float3 position) {
  const float time = scene.grid_iso_time_capacity.z;
  const float radius = scene.metaball_parameters.y;
  const float strength = scene.metaball_parameters.z;
  const uint count = (uint)scene.metaball_parameters.x;
  const float3 centers[5] = {
      float3(0.0, 0.1, 0.0),
      float3(sin(time * 0.9) * 0.75, cos(time * 0.7) * 0.4, 0.1),
      float3(cos(time * 0.6 + 1.3) * 0.7, sin(time * 0.8) * 0.5, -0.2),
      float3(sin(time * 0.5 + 2.1) * 0.65, -0.35, cos(time * 0.7) * 0.35),
      float3(-0.45, sin(time * 0.75 + 1.6) * 0.5, cos(time * 0.55) * 0.3)};
  float result = 0.0;
  [loop]
  for (uint index = 0; index < 5; ++index) {
    if (index >= count)
      break;
    const float3 delta = position - centers[index];
    result += exp(-dot(delta, delta) / max(radius * radius, 0.0001)) * strength;
  }
  return result;
}

float3 density_gradient(float3 position) {
  const float epsilon = 0.01;
  return float3(density(position + float3(epsilon, 0, 0)) -
                    density(position - float3(epsilon, 0, 0)),
                density(position + float3(0, epsilon, 0)) -
                    density(position - float3(0, epsilon, 0)),
                density(position + float3(0, 0, epsilon)) -
                    density(position - float3(0, 0, epsilon))) /
         (2.0 * epsilon);
}

[numthreads(4, 4, 4)]
void density_main(uint3 id : SV_DispatchThreadID) {
  const uint grid_size = (uint)scene.grid_iso_time_capacity.x;
  if (any(id >= grid_size))
    return;
  const float3 position = sample_position(id, grid_size);
  field_samples[sample_index(id, grid_size)] = float4(density_gradient(position), density(position));
}

static const uint2 edge_vertices[12] = {
    uint2(0, 1), uint2(1, 2), uint2(3, 2), uint2(0, 3), uint2(4, 5), uint2(5, 6),
    uint2(7, 6), uint2(4, 7), uint2(0, 4), uint2(1, 5), uint2(3, 7), uint2(2, 6)};
static const uint3 corner_offsets[8] = {
    uint3(0, 0, 0), uint3(1, 0, 0), uint3(1, 1, 0), uint3(0, 1, 0),
    uint3(0, 0, 1), uint3(1, 0, 1), uint3(1, 1, 1), uint3(0, 1, 1)};

[numthreads(4, 4, 4)]
void polygonize_main(uint3 id : SV_DispatchThreadID) {
  const uint grid_size = (uint)scene.grid_iso_time_capacity.x;
  if (any(id >= grid_size - 1))
    return;
  float4 corners[8];
  uint case_index = 0;
  [unroll]
  for (uint corner = 0; corner < 8; ++corner) {
    corners[corner] = field_samples[sample_index(id + corner_offsets[corner], grid_size)];
    if (corners[corner].w >= scene.grid_iso_time_capacity.y)
      case_index |= 1u << corner;
  }
  uint vertex_count = 0;
  [unroll]
  for (uint entry = 0; entry < 15; ++entry) {
    if (triangle_table[case_index * 16 + entry] < 0)
      break;
    ++vertex_count;
  }
  if (vertex_count == 0)
    return;
  uint base_vertex;
  generation_state.InterlockedAdd(0, vertex_count, base_vertex);
  const uint capacity = (uint)scene.grid_iso_time_capacity.w;
  if (base_vertex > capacity || vertex_count > capacity - base_vertex) {
    uint ignored;
    generation_state.InterlockedOr(4, 1, ignored);
    return;
  }
  [loop]
  for (uint entry = 0; entry < vertex_count; ++entry) {
    const uint edge = (uint)triangle_table[case_index * 16 + entry];
    const uint2 endpoints = edge_vertices[edge];
    const float4 first = corners[endpoints.x];
    const float4 second = corners[endpoints.y];
    const float denominator = second.w - first.w;
    const float interpolation = abs(denominator) < 0.00001
                                    ? 0.5
                                    : saturate((scene.grid_iso_time_capacity.y - first.w) /
                                               denominator);
    const float3 first_position =
        sample_position(id + corner_offsets[endpoints.x], grid_size);
    const float3 second_position =
        sample_position(id + corner_offsets[endpoints.y], grid_size);
    generated_vertex output;
    output.position = float4(lerp(first_position, second_position, interpolation), 1.0);
    output.normal = float4(normalize(-lerp(first.xyz, second.xyz, interpolation)), 0.0);
    generated_vertices[base_vertex + entry] = output;
  }
}

[numthreads(1, 1, 1)]
void finalize_main(uint3 id : SV_DispatchThreadID) {
  uint vertex_count = generation_state.Load(0);
  vertex_count = min(vertex_count, (uint)scene.grid_iso_time_capacity.w);
  vertex_count -= vertex_count % 3;
  generation_state.Store(16, vertex_count);
  generation_state.Store(20, 1);
  generation_state.Store(24, 0);
  generation_state.Store(28, 0);
}

struct vertex_output {
  float4 position : SV_Position;
  float3 world_position : TEXCOORD0;
  float3 normal : TEXCOORD1;
};

vertex_output vertex_main(float4 position : POSITION, float4 normal : NORMAL) {
  vertex_output output;
  output.position = mul(scene.view_projection, position);
  output.world_position = position.xyz;
  output.normal = normal.xyz;
  return output;
}

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float3 normal = normalize(input.normal);
  const float diffuse = saturate(dot(normal, normalize(float3(-0.4, 0.8, 0.5))));
  const float height = saturate(input.world_position.y * 0.35 + 0.5);
  const float3 base = lerp(float3(0.12, 0.4, 0.95), float3(0.95, 0.22, 0.5), height);
  return float4(base * (0.18 + diffuse * 0.82), 1.0);
}
