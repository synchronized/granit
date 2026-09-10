// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_COMMAND_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_COMMAND_ADAPTER_H_

#include <memory>
#include <span>

#include "backend/contracts/resources.h"
#include "backend/webgpu/context.h"

namespace granit::detail {

struct webgpu_command_context;

/** 将公共命令录制契约适配到 WebGPU Provider。 */
class webgpu_command_adapter {
public:
  webgpu_command_adapter(webgpu_context& context, webgpu_instance_handle instance);

  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource> allocate_recorder() const;
  [[nodiscard]] granit_result begin(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result
  begin_rendering(backend_command_recorder_resource& resource, webgpu_texture_view target,
                  webgpu_texture_view resolve_target, webgpu_load_operation load,
                  webgpu_store_operation store, const float clear[4],
                  webgpu_texture_view depth_target, webgpu_load_operation depth_load,
                  webgpu_store_operation depth_store, float clear_depth) const noexcept;
  [[nodiscard]] granit_result bind_pipeline(backend_command_recorder_resource& resource,
                                            webgpu_render_pipeline pipeline) const noexcept;
  [[nodiscard]] granit_result
  bind_graphics_groups(backend_command_recorder_resource& resource, webgpu_pipeline_layout layout,
                       std::uint32_t first_group, std::span<const webgpu_bind_group> groups,
                       std::span<const std::uint32_t> dynamic_offsets) const noexcept;
  [[nodiscard]] granit_result begin_compute(backend_command_recorder_resource&) const noexcept;
  [[nodiscard]] granit_result bind_compute_pipeline(backend_command_recorder_resource&,
                                                    webgpu_compute_pipeline) const noexcept;
  [[nodiscard]] granit_result bind_compute_groups(backend_command_recorder_resource&,
                                                  webgpu_pipeline_layout, std::uint32_t,
                                                  std::span<const webgpu_bind_group>,
                                                  std::span<const std::uint32_t>) const noexcept;
  [[nodiscard]] granit_result dispatch(backend_command_recorder_resource&, std::uint32_t,
                                       std::uint32_t, std::uint32_t) const noexcept;
  [[nodiscard]] granit_result end_compute(backend_command_recorder_resource&) const noexcept;
  [[nodiscard]] granit_result
  bind_vertex_buffers(backend_command_recorder_resource& resource, std::uint32_t first,
                      std::span<const webgpu_vertex_buffer_binding> bindings) const noexcept;
  [[nodiscard]] granit_result bind_index_buffer(backend_command_recorder_resource& resource,
                                                webgpu_buffer buffer, std::uint64_t offset,
                                                webgpu_index_format format) const noexcept;
  [[nodiscard]] granit_result
  set_viewports(backend_command_recorder_resource& resource, std::uint32_t first,
                std::span<const webgpu_viewport> viewports) const noexcept;
  [[nodiscard]] granit_result set_scissors(backend_command_recorder_resource& resource,
                                           std::uint32_t first,
                                           std::span<const webgpu_scissor> scissors) const noexcept;
  [[nodiscard]] granit_result copy_texture_to_buffer(backend_command_recorder_resource& resource,
                                                     webgpu_texture texture, webgpu_buffer buffer,
                                                     std::uint32_t width, std::uint32_t height,
                                                     std::uint32_t bytes_per_row) const noexcept;
  [[nodiscard]] granit_result
  copy_buffer(backend_command_recorder_resource& resource, webgpu_buffer source,
              webgpu_buffer destination,
              std::span<const webgpu_buffer_copy_region> regions) const noexcept;
  [[nodiscard]] granit_result
  copy_buffer_to_texture(backend_command_recorder_resource& resource, webgpu_buffer source,
                         webgpu_texture destination,
                         const webgpu_texture_buffer_copy& region) const noexcept;
  [[nodiscard]] granit_result
  copy_texture_to_buffer(backend_command_recorder_resource& resource, webgpu_texture source,
                         webgpu_buffer destination,
                         const webgpu_texture_buffer_copy& region) const noexcept;
  [[nodiscard]] granit_result copy_texture(backend_command_recorder_resource& resource,
                                           webgpu_texture source, webgpu_texture destination,
                                           const webgpu_texture_copy_region& region) const noexcept;
  [[nodiscard]] granit_result fill_buffer(backend_command_recorder_resource& resource,
                                          webgpu_buffer buffer, std::uint64_t offset,
                                          std::uint64_t size, std::uint32_t value) const noexcept;
  [[nodiscard]] granit_result
  generate_mipmaps(backend_command_recorder_resource& resource, webgpu_texture texture,
                   const webgpu_texture_mipmap_range& range) const noexcept;
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
  [[nodiscard]] webgpu_command_recorder
  native_recorder(backend_command_recorder_resource& resource) const noexcept;

private:
  std::shared_ptr<webgpu_command_context> context_;
};

} // namespace granit::detail

#endif
