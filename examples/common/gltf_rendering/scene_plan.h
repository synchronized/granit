// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_PLAN_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_PLAN_H_

#include "gltf/scene.h"

#include <granit/pipeline/scene.h>
#include <granit/renderer/sampler.hpp>

#include <cstdint>
#include <vector>

namespace granit::example::gltf_rendering {

struct packed_vertex {
  math::float3 position{};
  math::float3 normal{};
  math::float4 tangent{1, 0, 0, 1};
  math::float2 texture_coordinate{};
  math::float2 texture_coordinate_1{};
  math::float4 color{1, 1, 1, 1};
};

struct packed_primitive {
  std::uint64_t vertex_offset{};
  std::uint64_t index_offset{};
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::uint32_t material{gltf::invalid_index};
};

/** Node 展开后的稳定绘制身份；payload 零值保留为无效。 */
struct packed_draw {
  std::uint64_t payload{};
  std::uint32_t primitive{};
  std::uint32_t material{gltf::invalid_index};
  std::uint32_t node{};
  math::matrix4 model{math::identity_matrix4};
  math::matrix4 normal_matrix{math::identity_matrix4};
  math::float3 bounds_center{};
  float bounds_radius{};
};

struct texture_variant {
  std::uint32_t image{gltf::invalid_index};
  bool srgb{};

  friend bool operator==(const texture_variant&, const texture_variant&) = default;
};

struct sampler_key {
  granit::filter mag_filter{granit::filter::linear};
  granit::filter min_filter{granit::filter::linear};
  granit::mipmap_filter mip_filter{granit::mipmap_filter::linear};
  granit::address_mode address_u{granit::address_mode::repeat};
  granit::address_mode address_v{granit::address_mode::repeat};

  friend bool operator==(const sampler_key&, const sampler_key&) = default;
};

/** GPU 创建前的确定性打包结果，不包含 Renderer 句柄。 */
struct scene_plan {
  std::vector<packed_vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<packed_primitive> primitives;
  std::vector<packed_draw> draws;
  std::vector<granit_scene_renderable> renderables;
  std::vector<texture_variant> textures;
  std::vector<sampler_key> samplers;
  std::vector<std::uint32_t> source_sampler_to_plan;
};

enum class scene_plan_error { none, invalid_scene, numeric_overflow, out_of_memory };

/** 生成合并 Buffer 与纹理格式计划；失败时 output 保持不变。 */
[[nodiscard]] scene_plan_error build_scene_plan(const gltf::scene& source, scene_plan& output);

} // namespace granit::example::gltf_rendering

#endif // GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_PLAN_H_
