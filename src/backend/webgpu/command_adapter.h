// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_COMMAND_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_COMMAND_ADAPTER_H_

#include <memory>
#include <span>

#include "backend/plugin_loader.h"
#include "backend/resources.h"

namespace granit::detail {

struct webgpu_command_context;

/** 将公共命令录制契约适配到 WebGPU Provider。 */
class webgpu_command_adapter {
public:
  webgpu_command_adapter(backend_plugin_loader& loader, granit_backend_plugin_instance instance);

  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource> allocate_recorder() const;
  [[nodiscard]] granit_result begin(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result begin_rendering(backend_command_recorder_resource& resource,
                                              granit_backend_plugin_texture_view target,
                                              granit_backend_plugin_load_operation load,
                                              granit_backend_plugin_store_operation store,
                                              const float clear[4]) const noexcept;
  [[nodiscard]] granit_result
  bind_pipeline(backend_command_recorder_resource& resource,
                granit_backend_plugin_render_pipeline pipeline) const noexcept;
  [[nodiscard]] granit_result
  bind_graphics_groups(backend_command_recorder_resource& resource,
                       granit_backend_plugin_pipeline_layout layout, std::uint32_t first_group,
                       std::span<const granit_backend_plugin_bind_group> groups,
                       std::span<const std::uint32_t> dynamic_offsets) const noexcept;
  [[nodiscard]] granit_result begin_compute(backend_command_recorder_resource&) const noexcept;
  [[nodiscard]] granit_result
  bind_compute_pipeline(backend_command_recorder_resource&,
                        granit_backend_plugin_compute_pipeline) const noexcept;
  [[nodiscard]] granit_result bind_compute_groups(backend_command_recorder_resource&,
                                                  granit_backend_plugin_pipeline_layout,
                                                  std::uint32_t,
                                                  std::span<const granit_backend_plugin_bind_group>,
                                                  std::span<const std::uint32_t>) const noexcept;
  [[nodiscard]] granit_result dispatch(backend_command_recorder_resource&, std::uint32_t,
                                       std::uint32_t, std::uint32_t) const noexcept;
  [[nodiscard]] granit_result end_compute(backend_command_recorder_resource&) const noexcept;
  [[nodiscard]] granit_result bind_vertex_buffers(
      backend_command_recorder_resource& resource, std::uint32_t first,
      std::span<const granit_backend_plugin_vertex_buffer_binding> bindings) const noexcept;
  [[nodiscard]] granit_result
  bind_index_buffer(backend_command_recorder_resource& resource,
                    granit_backend_plugin_buffer buffer, std::uint64_t offset,
                    granit_backend_plugin_index_format format) const noexcept;
  [[nodiscard]] granit_result draw(backend_command_recorder_resource& resource,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex,
                                   std::uint32_t first_instance) const noexcept;
  [[nodiscard]] granit_result draw_indexed(backend_command_recorder_resource& resource,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance) const noexcept;
  [[nodiscard]] granit_result
  end_rendering(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] bool is_recording(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result end(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result submit(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result reset(backend_command_recorder_resource& resource) const noexcept;

private:
  std::shared_ptr<webgpu_command_context> context_;
};

} // namespace granit::detail

#endif
