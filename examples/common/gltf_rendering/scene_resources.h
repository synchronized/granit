// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_RESOURCES_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_RESOURCES_H_

#include "gltf/scene.h"
#include "gltf_rendering/material_update.h"
#include "gltf_rendering/scene_plan.h"
#include "gltf_rendering/scene_upload.h"

#include <granit/core/result.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline_warmup.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader_library.hpp>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace granit::example::gltf_rendering {

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

/** 与单个 Renderer 绑定的事务式 glTF Scene GPU 资源集合。 */
class scene_resources {
public:
  scene_resources() = default;
  ~scene_resources() = default;
  scene_resources(const scene_resources&) = delete;
  scene_resources& operator=(const scene_resources&) = delete;
  scene_resources(scene_resources&& other) noexcept;
  scene_resources& operator=(scene_resources&& other) noexcept;

  /** 成功后替换现有资源；失败时当前对象保持不变。 */
  [[nodiscard]] granit::result initialize(granit::renderer_ref renderer, const gltf::scene& source,
                                          float sampler_anisotropy = 8.0F,
                                          scene_upload_callback progress = nullptr,
                                          void* progress_user_data = nullptr);
  [[nodiscard]] granit::result initialize(granit::renderer& renderer, const gltf::scene& source,
                                          float sampler_anisotropy = 8.0F,
                                          scene_upload_callback progress = nullptr,
                                          void* progress_user_data = nullptr) {
    return initialize(renderer.ref(), source, sampler_anisotropy, progress, progress_user_data);
  }
  /** 使用工作线程预先生成的计划创建资源；plan 在失败时仍会被消费。 */
  [[nodiscard]] granit::result initialize(granit::renderer_ref renderer, const gltf::scene& source,
                                          scene_plan plan, float sampler_anisotropy = 8.0F,
                                          scene_upload_callback progress = nullptr,
                                          void* progress_user_data = nullptr);
  [[nodiscard]] granit::result initialize(granit::renderer& renderer, const gltf::scene& source,
                                          scene_plan plan, float sampler_anisotropy = 8.0F,
                                          scene_upload_callback progress = nullptr,
                                          void* progress_user_data = nullptr) {
    return initialize(renderer.ref(), source, std::move(plan), sampler_anisotropy, progress,
                      progress_user_data);
  }
  void reset() noexcept;

  [[nodiscard]] bool valid() const noexcept { return renderer_.valid(); }
  [[nodiscard]] const scene_plan& plan() const noexcept { return plan_; }
  [[nodiscard]] const std::vector<gpu_texture>& textures() const noexcept { return textures_; }
  [[nodiscard]] const std::vector<granit::mesh>& meshes() const noexcept { return meshes_; }
  [[nodiscard]] const std::vector<granit::sampler>& samplers() const noexcept { return samplers_; }
  [[nodiscard]] const std::vector<granit::material_instance>& materials() const noexcept {
    return materials_;
  }
  [[nodiscard]] const std::vector<granit::render_pipeline_draw_binding>&
  draw_bindings() const noexcept {
    return draw_bindings_;
  }
  [[nodiscard]] std::span<const granit_scene_renderable> renderables() const noexcept {
    return plan_.renderables;
  }

  /** 将当前场景使用的标准 PBR 变体加入预热批次，返回与条目一一对应的结果索引。 */
  [[nodiscard]] granit::result
  add_pipeline_warmups(pipeline_warmup_batch_ref batch, texture_format color_format,
                       sample_count samples, std::vector<std::uint32_t>& result_indices) noexcept;

  /** 查询 Inspector 缩略图使用的实际纹理绑定，不转移资源所有权。 */
  [[nodiscard]] granit::result texture_binding(const gltf::texture_reference& reference, bool srgb,
                                               granit::texture_view_ref& view,
                                               granit::sampler_ref& sampler) const noexcept;

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
                                                       const material_factor_update& edit) noexcept;

  /** 更新所有材质的调试显示模式。 */
  [[nodiscard]] granit::result update_debug_display(std::uint32_t mode) noexcept;

private:
  [[nodiscard]] granit::result create(granit::renderer_ref renderer, const gltf::scene& source,
                                      scene_plan plan, float sampler_anisotropy,
                                      scene_upload_callback progress, void* progress_user_data);

  granit::renderer_ref renderer_;
  scene_plan plan_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  std::vector<gpu_texture> textures_;
  std::vector<granit::sampler> samplers_;
  std::vector<granit::mesh> meshes_;
  default_material_textures default_textures_;
  granit::sampler default_sampler_;
  granit::shader_library shader_library_;
  std::vector<granit::material_instance> materials_;
  std::vector<gltf::material_alpha_mode> material_alpha_modes_;
  std::vector<granit::render_pipeline_draw_binding> draw_bindings_;
};

} // namespace granit::example::gltf_rendering

#endif
