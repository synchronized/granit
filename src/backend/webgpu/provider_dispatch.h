// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEBGPU_PROVIDER_LOADER_H_
#define GRANIT_WEBGPU_PROVIDER_LOADER_H_

#include <cstdint>
#include <span>
#include <vector>

#include <granit/core/result.h>

#include "backend/webgpu/provider_api.h"

namespace granit::detail {

/** 连接并校验 Emscripten WebGPU 静态 Provider。 */
class webgpu_provider_dispatch {
public:
  webgpu_provider_dispatch() = default;
  ~webgpu_provider_dispatch();

  webgpu_provider_dispatch(const webgpu_provider_dispatch&) = delete;
  webgpu_provider_dispatch& operator=(const webgpu_provider_dispatch&) = delete;

  /** 校验并接入静态 WebGPU 实现。 */
  [[nodiscard]] granit_result connect(const granit_webgpu_provider_api* api) noexcept;
  [[nodiscard]] granit_result
  create_instance(const granit_webgpu_provider_host_api* host,
                  granit_webgpu_provider_instance* out_instance) noexcept;
  [[nodiscard]] granit_result destroy_instance(granit_webgpu_provider_instance instance) noexcept;
  [[nodiscard]] granit_result
  get_capabilities(granit_webgpu_provider_instance instance,
                   granit_webgpu_provider_capabilities* capabilities) noexcept;
  [[nodiscard]] granit_result
  get_instance_status(granit_webgpu_provider_instance instance,
                      granit_webgpu_provider_instance_status* status) noexcept;
  [[nodiscard]] granit_result process_events(granit_webgpu_provider_instance instance) noexcept;
  [[nodiscard]] granit_result
  create_win32_surface(granit_webgpu_provider_instance instance,
                       const granit_webgpu_provider_win32_surface_desc* desc,
                       granit_webgpu_provider_surface* surface) noexcept;
  [[nodiscard]] granit_result
  create_xcb_surface(granit_webgpu_provider_instance instance,
                     const granit_webgpu_provider_xcb_surface_desc* desc,
                     granit_webgpu_provider_surface* surface) noexcept;
  [[nodiscard]] granit_result
  create_wayland_surface(granit_webgpu_provider_instance instance,
                         const granit_webgpu_provider_wayland_surface_desc* desc,
                         granit_webgpu_provider_surface* surface) noexcept;
  [[nodiscard]] granit_result
  create_canvas_surface(granit_webgpu_provider_instance instance,
                        const granit_webgpu_provider_canvas_surface_desc* desc,
                        granit_webgpu_provider_surface* surface) noexcept;
  [[nodiscard]] granit_result destroy_surface(granit_webgpu_provider_instance instance,
                                              granit_webgpu_provider_surface surface) noexcept;
  [[nodiscard]] granit_result
  create_swapchain(granit_webgpu_provider_instance instance, granit_webgpu_provider_surface surface,
                   const granit_webgpu_provider_swapchain_desc* desc,
                   granit_webgpu_provider_swapchain* swapchain) noexcept;
  [[nodiscard]] granit_result
  recreate_swapchain(granit_webgpu_provider_instance instance,
                     granit_webgpu_provider_swapchain swapchain,
                     const granit_webgpu_provider_swapchain_desc* desc) noexcept;
  [[nodiscard]] granit_result
  get_swapchain_info(granit_webgpu_provider_instance instance,
                     granit_webgpu_provider_swapchain swapchain,
                     granit_webgpu_provider_swapchain_info* info) noexcept;
  [[nodiscard]] granit_result
  acquire_swapchain(granit_webgpu_provider_instance instance,
                    granit_webgpu_provider_swapchain swapchain,
                    granit_webgpu_provider_acquired_frame* frame) noexcept;
  [[nodiscard]] granit_result present_swapchain(granit_webgpu_provider_instance instance,
                                                granit_webgpu_provider_swapchain swapchain,
                                                std::uint32_t* needs_recreate) noexcept;
  [[nodiscard]] granit_result cancel_swapchain(granit_webgpu_provider_instance instance,
                                               granit_webgpu_provider_swapchain swapchain,
                                               std::uint32_t* needs_recreate) noexcept;
  [[nodiscard]] granit_result
  destroy_swapchain(granit_webgpu_provider_instance instance,
                    granit_webgpu_provider_swapchain swapchain) noexcept;
  [[nodiscard]] granit_result create_buffer(granit_webgpu_provider_instance instance,
                                            const granit_webgpu_provider_buffer_desc* desc,
                                            granit_webgpu_provider_buffer* buffer) noexcept;
  [[nodiscard]] granit_result destroy_buffer(granit_webgpu_provider_instance instance,
                                             granit_webgpu_provider_buffer buffer) noexcept;
  [[nodiscard]] granit_result write_buffer(granit_webgpu_provider_instance instance,
                                           granit_webgpu_provider_buffer buffer,
                                           std::uint64_t offset, const void* data,
                                           std::uint64_t size) noexcept;
  [[nodiscard]] granit_result read_buffer(granit_webgpu_provider_instance instance,
                                          granit_webgpu_provider_buffer buffer,
                                          std::uint64_t offset, void* data,
                                          std::uint64_t size) noexcept;
  [[nodiscard]] granit_result create_texture(granit_webgpu_provider_instance instance,
                                             const granit_webgpu_provider_texture_desc* desc,
                                             granit_webgpu_provider_texture* texture) noexcept;
  [[nodiscard]] granit_result destroy_texture(granit_webgpu_provider_instance instance,
                                              granit_webgpu_provider_texture texture) noexcept;
  [[nodiscard]] granit_result write_texture(granit_webgpu_provider_instance instance,
                                            granit_webgpu_provider_texture texture,
                                            const granit_webgpu_provider_texture_write_desc* desc,
                                            const void* data, std::uint64_t size) noexcept;
  [[nodiscard]] granit_result
  write_upload_batch(granit_webgpu_provider_instance instance,
                     std::span<const granit_webgpu_provider_upload_operation> operations) noexcept;
  [[nodiscard]] granit_result
  create_texture_view(granit_webgpu_provider_instance instance,
                      granit_webgpu_provider_texture texture,
                      const granit_webgpu_provider_texture_view_desc* desc,
                      granit_webgpu_provider_texture_view* view) noexcept;
  [[nodiscard]] granit_result
  destroy_texture_view(granit_webgpu_provider_instance instance,
                       granit_webgpu_provider_texture_view view) noexcept;
  [[nodiscard]] granit_result create_sampler(granit_webgpu_provider_instance instance,
                                             const granit_webgpu_provider_sampler_desc* desc,
                                             granit_webgpu_provider_sampler* sampler) noexcept;
  [[nodiscard]] granit_result destroy_sampler(granit_webgpu_provider_instance instance,
                                              granit_webgpu_provider_sampler sampler) noexcept;
  [[nodiscard]] granit_result
  create_bind_group_layout(granit_webgpu_provider_instance instance,
                           const granit_webgpu_provider_bind_group_layout_desc* desc,
                           granit_webgpu_provider_bind_group_layout* layout) noexcept;
  [[nodiscard]] granit_result
  destroy_bind_group_layout(granit_webgpu_provider_instance instance,
                            granit_webgpu_provider_bind_group_layout layout) noexcept;
  [[nodiscard]] granit_result
  create_bind_group(granit_webgpu_provider_instance instance,
                    const granit_webgpu_provider_bind_group_desc* desc,
                    granit_webgpu_provider_bind_group* bind_group) noexcept;
  [[nodiscard]] granit_result
  destroy_bind_group(granit_webgpu_provider_instance instance,
                     granit_webgpu_provider_bind_group bind_group) noexcept;
  [[nodiscard]] granit_result create_shader(granit_webgpu_provider_instance instance,
                                            const granit_webgpu_provider_shader_desc* desc,
                                            granit_webgpu_provider_shader* shader) noexcept;
  [[nodiscard]] granit_result destroy_shader(granit_webgpu_provider_instance instance,
                                             granit_webgpu_provider_shader shader) noexcept;
  [[nodiscard]] granit_result
  create_pipeline_layout(granit_webgpu_provider_instance instance,
                         const granit_webgpu_provider_pipeline_layout_desc* desc,
                         granit_webgpu_provider_pipeline_layout* pipeline_layout) noexcept;
  [[nodiscard]] granit_result
  destroy_pipeline_layout(granit_webgpu_provider_instance instance,
                          granit_webgpu_provider_pipeline_layout pipeline_layout) noexcept;
  [[nodiscard]] granit_result
  create_compute_pipeline(granit_webgpu_provider_instance instance,
                          const granit_webgpu_provider_compute_pipeline_desc* desc,
                          granit_webgpu_provider_compute_pipeline* pipeline) noexcept;
  [[nodiscard]] granit_result
  destroy_compute_pipeline(granit_webgpu_provider_instance instance,
                           granit_webgpu_provider_compute_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  recorder_begin_compute(granit_webgpu_provider_instance instance,
                         granit_webgpu_provider_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_compute_pipeline(granit_webgpu_provider_instance instance,
                                 granit_webgpu_provider_command_recorder recorder,
                                 granit_webgpu_provider_compute_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result recorder_bind_compute_groups(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_pipeline_layout layout, std::uint32_t first_group,
      std::span<const granit_webgpu_provider_bind_group> groups,
      std::span<const std::uint32_t> dynamic_offsets) noexcept;
  [[nodiscard]] granit_result recorder_dispatch(granit_webgpu_provider_instance instance,
                                                granit_webgpu_provider_command_recorder recorder,
                                                std::uint32_t x, std::uint32_t y,
                                                std::uint32_t z) noexcept;
  [[nodiscard]] granit_result
  recorder_end_compute(granit_webgpu_provider_instance instance,
                       granit_webgpu_provider_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  create_render_pipeline(granit_webgpu_provider_instance instance,
                         const granit_webgpu_provider_render_pipeline_desc* desc,
                         granit_webgpu_provider_render_pipeline* render_pipeline) noexcept;
  [[nodiscard]] granit_result
  destroy_render_pipeline(granit_webgpu_provider_instance instance,
                          granit_webgpu_provider_render_pipeline render_pipeline) noexcept;
  [[nodiscard]] granit_result
  create_command_recorder(granit_webgpu_provider_instance instance,
                          granit_webgpu_provider_command_recorder* recorder) noexcept;
  [[nodiscard]] granit_result
  destroy_command_recorder(granit_webgpu_provider_instance instance,
                           granit_webgpu_provider_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result recorder_copy_buffer_to_texture(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_buffer buffer, granit_webgpu_provider_texture texture,
      std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept;
  [[nodiscard]] granit_result recorder_begin_rendering(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_texture_view target, granit_webgpu_provider_load_operation load,
      granit_webgpu_provider_store_operation store, const float clear[4],
      granit_webgpu_provider_texture_view resolve_target = 0,
      granit_webgpu_provider_texture_view depth_target = 0,
      granit_webgpu_provider_load_operation depth_load =
          GRANIT_WEBGPU_PROVIDER_LOAD_OPERATION_CLEAR,
      granit_webgpu_provider_store_operation depth_store =
          GRANIT_WEBGPU_PROVIDER_STORE_OPERATION_DISCARD,
      float clear_depth = 1.0F) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_pipeline(granit_webgpu_provider_instance instance,
                         granit_webgpu_provider_command_recorder recorder,
                         granit_webgpu_provider_render_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result recorder_bind_graphics_groups(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_pipeline_layout layout, std::uint32_t first_group,
      std::span<const granit_webgpu_provider_bind_group> groups,
      std::span<const std::uint32_t> dynamic_offsets) noexcept;
  [[nodiscard]] granit_result recorder_bind_vertex_buffers(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      std::uint32_t first,
      std::span<const granit_webgpu_provider_vertex_buffer_binding> bindings) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_index_buffer(granit_webgpu_provider_instance instance,
                             granit_webgpu_provider_command_recorder recorder,
                             granit_webgpu_provider_buffer buffer, std::uint64_t offset,
                             granit_webgpu_provider_index_format format) noexcept;
  [[nodiscard]] granit_result
  recorder_set_viewports(granit_webgpu_provider_instance instance,
                         granit_webgpu_provider_command_recorder recorder, std::uint32_t first,
                         std::span<const granit_webgpu_provider_viewport> viewports) noexcept;
  [[nodiscard]] granit_result
  recorder_set_scissors(granit_webgpu_provider_instance instance,
                        granit_webgpu_provider_command_recorder recorder, std::uint32_t first,
                        std::span<const granit_webgpu_provider_scissor> scissors) noexcept;
  [[nodiscard]] granit_result
  recorder_draw_vertices(granit_webgpu_provider_instance instance,
                         granit_webgpu_provider_command_recorder recorder,
                         std::uint32_t vertex_count, std::uint32_t instance_count,
                         std::uint32_t first_vertex, std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  recorder_draw_indices(granit_webgpu_provider_instance instance,
                        granit_webgpu_provider_command_recorder recorder, std::uint32_t index_count,
                        std::uint32_t instance_count, std::uint32_t first_index,
                        std::int32_t vertex_offset, std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  recorder_end_rendering(granit_webgpu_provider_instance instance,
                         granit_webgpu_provider_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  finish_command_recorder(granit_webgpu_provider_instance instance,
                          granit_webgpu_provider_command_recorder recorder,
                          granit_webgpu_provider_command_buffer* command_buffer) noexcept;
  [[nodiscard]] granit_result
  destroy_command_buffer(granit_webgpu_provider_instance instance,
                         granit_webgpu_provider_command_buffer command_buffer) noexcept;
  [[nodiscard]] granit_result
  submit_command_buffer(granit_webgpu_provider_instance instance,
                        granit_webgpu_provider_command_buffer command_buffer) noexcept;
  [[nodiscard]] granit_result recorder_copy_texture_to_buffer(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_texture texture, granit_webgpu_provider_buffer buffer,
      std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept;
  [[nodiscard]] granit_result recorder_copy_buffer(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_buffer source, granit_webgpu_provider_buffer destination,
      std::span<const granit_webgpu_provider_buffer_copy_region> regions) noexcept;
  [[nodiscard]] granit_result recorder_copy_buffer_to_texture_v2(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_buffer source, granit_webgpu_provider_texture destination,
      const granit_webgpu_provider_texture_buffer_copy& region) noexcept;
  [[nodiscard]] granit_result recorder_copy_texture_to_buffer_v2(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_texture source, granit_webgpu_provider_buffer destination,
      const granit_webgpu_provider_texture_buffer_copy& region) noexcept;
  [[nodiscard]] granit_result recorder_copy_texture(
      granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
      granit_webgpu_provider_texture source, granit_webgpu_provider_texture destination,
      const granit_webgpu_provider_texture_copy_region& region) noexcept;
  [[nodiscard]] granit_result recorder_fill_buffer(granit_webgpu_provider_instance instance,
                                                   granit_webgpu_provider_command_recorder recorder,
                                                   granit_webgpu_provider_buffer buffer,
                                                   std::uint64_t offset, std::uint64_t size,
                                                   std::uint32_t value) noexcept;
  [[nodiscard]] granit_result
  recorder_generate_mipmaps(granit_webgpu_provider_instance instance,
                            granit_webgpu_provider_command_recorder recorder,
                            granit_webgpu_provider_texture texture,
                            const granit_webgpu_provider_texture_mipmap_range& range) noexcept;
  void close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return api_ != nullptr; }
  [[nodiscard]] const granit_webgpu_provider_api* api() const noexcept { return api_; }

private:
  const granit_webgpu_provider_api* api_{nullptr};
  std::vector<granit_webgpu_provider_instance> instances_;
};

} // namespace granit::detail

#endif
