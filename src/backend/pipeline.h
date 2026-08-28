// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PIPELINE_H_
#define GRANIT_BACKEND_PIPELINE_H_

#include <memory>
#include <span>

#include <granit/renderer/pipeline.h>

#include "backend/resources.h"

namespace granit::detail {

/** 提供后端中立的 Pipeline Layout 创建能力。 */
class backend_pipeline_layout_renderer {
public:
  backend_pipeline_layout_renderer() = default;
  virtual ~backend_pipeline_layout_renderer() = default;
  backend_pipeline_layout_renderer(const backend_pipeline_layout_renderer&) = delete;
  backend_pipeline_layout_renderer& operator=(const backend_pipeline_layout_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_pipeline_layout_resource>
  allocate_pipeline_layout_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_pipeline_layout(std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
                         backend_pipeline_layout_resource& layout) noexcept = 0;
};

/** 提供 Web MVP 当前所需的无绑定 Pipeline 创建能力。 */
class backend_pipeline_renderer : public backend_pipeline_layout_renderer {
public:
  backend_pipeline_renderer() = default;
  virtual ~backend_pipeline_renderer() = default;
  backend_pipeline_renderer(const backend_pipeline_renderer&) = delete;
  backend_pipeline_renderer& operator=(const backend_pipeline_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_graphics_pipeline_resource>
  allocate_graphics_pipeline_resource() = 0;
  [[nodiscard]] virtual granit_result
  validate_graphics_pipeline(const granit_graphics_pipeline_desc& desc) const noexcept = 0;
  [[nodiscard]] virtual granit_result create_graphics_pipeline(
      backend_graphics_pipeline_resource& pipeline, backend_pipeline_layout_resource& layout,
      backend_shader_resource& vertex_shader, backend_shader_resource& fragment_shader,
      granit_texture_format color_format) noexcept = 0;
};

} // namespace granit::detail

#endif
