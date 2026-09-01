// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_GPU_SCENE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_GPU_SCENE_H_

#include "gltf/scene.h"

#include <cstdint>
#include <vector>

namespace granit::example::model_viewer {

struct packed_vertex {
  math::float3 position{};
  math::float3 normal{};
  math::float4 tangent{1, 0, 0, 1};
  math::float2 texture_coordinate{};
};

struct packed_primitive {
  std::uint64_t vertex_offset{};
  std::uint64_t index_offset{};
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::uint32_t material{gltf::invalid_index};
};

struct texture_variant {
  std::uint32_t image{gltf::invalid_index};
  bool srgb{};

  friend bool operator==(const texture_variant&, const texture_variant&) = default;
};

/** GPU 创建前的确定性打包结果，不包含 Renderer 句柄。 */
struct gpu_scene_plan {
  std::vector<packed_vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<packed_primitive> primitives;
  std::vector<texture_variant> textures;
};

enum class gpu_scene_plan_error { none, invalid_scene, numeric_overflow, out_of_memory };

/** 生成合并 Buffer 与纹理格式计划；失败时 output 保持不变。 */
[[nodiscard]] gpu_scene_plan_error build_gpu_scene_plan(const gltf::scene& source,
                                                        gpu_scene_plan& output);

} // namespace granit::example::model_viewer

#endif
