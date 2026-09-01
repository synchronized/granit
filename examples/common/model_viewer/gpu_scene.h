// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_GPU_SCENE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_GPU_SCENE_H_

#include "gltf/scene.h"
#include "model_viewer/material_edit.h"

#include <granit/core/result.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.h>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <span>
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
struct gpu_scene_plan {
  std::vector<packed_vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<packed_primitive> primitives;
  std::vector<packed_draw> draws;
  std::vector<granit_scene_renderable> renderables;
  std::vector<texture_variant> textures;
  std::vector<sampler_key> samplers;
  std::vector<std::uint32_t> source_sampler_to_plan;
};

enum class gpu_scene_plan_error { none, invalid_scene, numeric_overflow, out_of_memory };

/** 生成合并 Buffer 与纹理格式计划；失败时 output 保持不变。 */
[[nodiscard]] gpu_scene_plan_error build_gpu_scene_plan(const gltf::scene& source,
                                                        gpu_scene_plan& output);

struct gpu_texture {
  texture_variant variant{};
  granit::texture texture;
  granit::texture_view view;
};

struct default_material_textures {
  gpu_texture white_srgb;
  gpu_texture white_linear;
  gpu_texture normal_linear;
};

/** 与单个 Renderer 绑定的事务式 GPU Scene 资源集合。 */
class gpu_scene {
public:
  gpu_scene() = default;
  ~gpu_scene() = default;
  gpu_scene(const gpu_scene&) = delete;
  gpu_scene& operator=(const gpu_scene&) = delete;
  gpu_scene(gpu_scene&& other) noexcept;
  gpu_scene& operator=(gpu_scene&& other) noexcept;

  /** 成功后替换现有资源；失败时当前对象保持不变。 */
  [[nodiscard]] granit::result initialize(granit_renderer renderer, const gltf::scene& source);
  void reset() noexcept;

  [[nodiscard]] bool valid() const noexcept { return renderer_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] const gpu_scene_plan& plan() const noexcept { return plan_; }
  [[nodiscard]] const std::vector<gpu_texture>& textures() const noexcept { return textures_; }
  [[nodiscard]] const std::vector<granit::mesh>& meshes() const noexcept { return meshes_; }
  [[nodiscard]] const std::vector<granit::sampler>& samplers() const noexcept { return samplers_; }
  [[nodiscard]] const std::vector<granit::material_instance>& materials() const noexcept {
    return materials_;
  }
  [[nodiscard]] const std::vector<granit_render_pipeline_draw_binding>&
  draw_bindings() const noexcept {
    return draw_bindings_;
  }
  [[nodiscard]] std::span<const granit_scene_renderable> renderables() const noexcept {
    return plan_.renderables;
  }

  /** 查询 Inspector 缩略图使用的实际纹理绑定，不转移资源所有权。 */
  [[nodiscard]] granit::result texture_binding(const gltf::texture_reference& reference, bool srgb,
                                               granit_texture_view& view,
                                               granit_sampler& sampler) const noexcept;

  /** 使用当前稳定 Renderable 与调用方逐帧 View/Light 创建不可变场景快照。 */
  [[nodiscard]] granit::result
  create_snapshot(std::span<const granit_scene_view> views,
                  std::span<const granit_scene_directional_light> directional_lights,
                  std::span<const granit_scene_point_light> point_lights,
                  std::span<const granit_scene_spot_light> spot_lights,
                  granit::scene_snapshot& output) const noexcept;

  /** 事务式更新 GPU 参数；成功后才同步修改 CPU Scene。 */
  [[nodiscard]] granit::result update_material_factors(gltf::scene& source,
                                                       std::uint32_t material_index,
                                                       const material_factor_edit& edit) noexcept;

private:
  [[nodiscard]] granit::result create(granit_renderer renderer, const gltf::scene& source);

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  gpu_scene_plan plan_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  std::vector<gpu_texture> textures_;
  std::vector<granit::sampler> samplers_;
  std::vector<granit::mesh> meshes_;
  default_material_textures default_textures_;
  granit::sampler default_sampler_;
  std::vector<granit::material_instance> materials_;
  std::vector<granit_render_pipeline_draw_binding> draw_bindings_;
};

} // namespace granit::example::model_viewer

#endif
