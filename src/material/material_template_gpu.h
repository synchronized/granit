// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_TEMPLATE_GPU_H
#define GRANIT_MATERIAL_MATERIAL_TEMPLATE_GPU_H

#include "material/material_package.h"

#include <granit/core/result.h>
#include <granit/pipeline/material.h>
#include <granit/renderer/pipeline.h>

#include <mutex>
#include <span>
#include <vector>

namespace granit::material {

struct material_pipeline_request {
  material_pass_id pass = 0;
  material_variant_key variant = 0;
  granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  granit_texture_format depth_stencil_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  granit_sample_count sample_count = GRANIT_SAMPLE_COUNT_1;
};

class material_template_gpu {
public:
  material_template_gpu() = default;
  ~material_template_gpu();
  material_template_gpu(const material_template_gpu&) = delete;
  material_template_gpu& operator=(const material_template_gpu&) = delete;

  /** additional_layouts 从 Group 2 起追加，调用期间借用。 */
  [[nodiscard]] granit_result
  initialize(granit_renderer renderer, const material_package& package,
             std::span<const granit_bind_group_layout> additional_layouts = {},
             granit_material_shader_resolver shader_resolver = nullptr,
             void* shader_resolver_user_data = nullptr);
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] granit_result acquire_pipeline(const material_pipeline_request& request,
                                               granit_graphics_pipeline& pipeline);

  [[nodiscard]] granit_bind_group_layout material_layout() const noexcept {
    return material_layout_;
  }
  [[nodiscard]] granit_bind_group_layout frame_layout() const noexcept { return frame_layout_; }
  [[nodiscard]] granit_pipeline_layout pipeline_layout() const noexcept { return pipeline_layout_; }
  [[nodiscard]] std::size_t cached_pipeline_count() const noexcept;

private:
  struct cache_entry {
    material_pipeline_request request;
    granit_shader vertex_shader = GRANIT_NULL_HANDLE;
    granit_shader fragment_shader = GRANIT_NULL_HANDLE;
    granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  };

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  const material_package* package_ = nullptr;
  granit_bind_group_layout frame_layout_ = GRANIT_NULL_HANDLE;
  granit_bind_group_layout material_layout_ = GRANIT_NULL_HANDLE;
  granit_pipeline_layout pipeline_layout_ = GRANIT_NULL_HANDLE;
  granit_material_shader_resolver shader_resolver_ = nullptr;
  void* shader_resolver_user_data_ = nullptr;
  mutable std::mutex mutex_;
  std::vector<cache_entry> cache_;
};

} // namespace granit::material

#endif
