// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_PIPELINE_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_PIPELINE_ADAPTER_H_

#include <memory>
#include <span>

#include <granit/renderer/pipeline.h>

#include "backend/plugin_loader.h"
#include "backend/resources.h"

namespace granit::detail {

struct webgpu_pipeline_context;

/** 适配 WebGPU MVP 的无绑定 Pipeline Layout 与基础图形 Pipeline。 */
class webgpu_pipeline_adapter {
public:
  webgpu_pipeline_adapter(backend_plugin_loader& loader, granit_backend_plugin_instance instance);

  [[nodiscard]] std::unique_ptr<backend_pipeline_layout_resource> allocate_pipeline_layout() const;
  [[nodiscard]] std::unique_ptr<backend_graphics_pipeline_resource>
  allocate_graphics_pipeline() const;
  [[nodiscard]] granit_result
  validate_graphics_pipeline(const granit_graphics_pipeline_desc& desc) const noexcept;
  [[nodiscard]] granit_result
  create_pipeline_layout(std::span<const granit_backend_plugin_bind_group_layout> layouts,
                         backend_pipeline_layout_resource& resource) const noexcept;
  [[nodiscard]] granit_result create_graphics_pipeline(
      backend_graphics_pipeline_resource& resource, backend_pipeline_layout_resource& layout,
      granit_backend_plugin_shader vertex_shader, granit_backend_plugin_shader fragment_shader,
      std::span<const granit_vertex_buffer_layout> vertex_buffers,
      granit_texture_format color_format) const noexcept;
  [[nodiscard]] granit_backend_plugin_render_pipeline
  native_handle(backend_graphics_pipeline_resource& resource) const noexcept;

private:
  std::shared_ptr<webgpu_pipeline_context> context_;
};

} // namespace granit::detail

#endif
