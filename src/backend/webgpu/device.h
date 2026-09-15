// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEBGPU_DEVICE_H_
#define GRANIT_WEBGPU_DEVICE_H_

#include <cstdint>
#include <span>

#include <granit/core/result.h>
#include <granit/renderer/native_surface.h>

#include "backend/webgpu/types.h"

namespace granit::detail {

/** 拥有 Emscripten WebGPU 实例、设备生命周期和原生资源操作。 */
class webgpu_device {
public:
  webgpu_device() = default;
  ~webgpu_device();

  webgpu_device(const webgpu_device&) = delete;
  webgpu_device& operator=(const webgpu_device&) = delete;

  /** 接入随当前后端静态编译的 WebGPU 实现。 */
  [[nodiscard]] granit_result open() noexcept;
  [[nodiscard]] granit_result create_instance(const webgpu_host_api* host) noexcept;
  [[nodiscard]] granit_result destroy_instance() noexcept;
  [[nodiscard]] granit_result get_capabilities(webgpu_capabilities* capabilities) noexcept;
  [[nodiscard]] granit_result get_instance_status(webgpu_instance_status* status) noexcept;
  [[nodiscard]] granit_result process_events() noexcept;
  [[nodiscard]] granit_result create_surface(const granit_surface_desc* desc,
                                             webgpu_surface* surface) noexcept;
  [[nodiscard]] granit_result destroy_surface(webgpu_surface surface) noexcept;
  [[nodiscard]] granit_result create_swapchain(webgpu_surface surface,
                                               const webgpu_swapchain_desc* desc,
                                               webgpu_swapchain* swapchain) noexcept;
  [[nodiscard]] granit_result recreate_swapchain(webgpu_swapchain swapchain,
                                                 const webgpu_swapchain_desc* desc) noexcept;
  [[nodiscard]] granit_result get_swapchain_info(webgpu_swapchain swapchain,
                                                 webgpu_swapchain_info* info) noexcept;
  [[nodiscard]] granit_result acquire_swapchain(webgpu_swapchain swapchain,
                                                webgpu_acquired_frame* frame) noexcept;
  [[nodiscard]] granit_result present_swapchain(webgpu_swapchain swapchain,
                                                std::uint32_t* needs_recreate) noexcept;
  [[nodiscard]] granit_result cancel_swapchain(webgpu_swapchain swapchain,
                                               std::uint32_t* needs_recreate) noexcept;
  [[nodiscard]] granit_result destroy_swapchain(webgpu_swapchain swapchain) noexcept;
  [[nodiscard]] granit_result create_buffer(const webgpu_buffer_desc* desc,
                                            webgpu_buffer* buffer) noexcept;
  [[nodiscard]] granit_result destroy_buffer(webgpu_buffer buffer) noexcept;
  [[nodiscard]] granit_result write_buffer(webgpu_buffer buffer, std::uint64_t offset,
                                           const void* data, std::uint64_t size) noexcept;
  [[nodiscard]] granit_result read_buffer(webgpu_buffer buffer, std::uint64_t offset, void* data,
                                          std::uint64_t size) noexcept;
  [[nodiscard]] granit_result begin_readback(webgpu_buffer buffer, std::uint64_t offset,
                                             std::uint64_t size,
                                             webgpu_readback* readback) noexcept;
  [[nodiscard]] granit_result poll_readback(webgpu_readback readback) noexcept;
  [[nodiscard]] granit_result copy_readback(webgpu_readback readback, std::uint64_t offset,
                                            void* data, std::uint64_t size) noexcept;
  [[nodiscard]] granit_result destroy_readback(webgpu_readback readback) noexcept;
  [[nodiscard]] granit_result create_texture(const webgpu_texture_desc* desc,
                                             webgpu_texture* texture) noexcept;
  [[nodiscard]] granit_result destroy_texture(webgpu_texture texture) noexcept;
  [[nodiscard]] granit_result write_texture(webgpu_texture texture,
                                            const webgpu_texture_write_desc* desc, const void* data,
                                            std::uint64_t size) noexcept;
  [[nodiscard]] granit_result
  write_upload_batch(std::span<const webgpu_upload_operation> operations) noexcept;
  [[nodiscard]] granit_result create_texture_view(webgpu_texture texture,
                                                  const webgpu_texture_view_desc* desc,
                                                  webgpu_texture_view* view) noexcept;
  [[nodiscard]] granit_result destroy_texture_view(webgpu_texture_view view) noexcept;
  [[nodiscard]] granit_result create_sampler(const webgpu_sampler_desc* desc,
                                             webgpu_sampler* sampler) noexcept;
  [[nodiscard]] granit_result destroy_sampler(webgpu_sampler sampler) noexcept;
  [[nodiscard]] granit_result create_bind_group_layout(const webgpu_bind_group_layout_desc* desc,
                                                       webgpu_bind_group_layout* layout) noexcept;
  [[nodiscard]] granit_result destroy_bind_group_layout(webgpu_bind_group_layout layout) noexcept;
  [[nodiscard]] granit_result create_bind_group(const webgpu_bind_group_desc* desc,
                                                webgpu_bind_group* bind_group) noexcept;
  [[nodiscard]] granit_result destroy_bind_group(webgpu_bind_group bind_group) noexcept;
  [[nodiscard]] granit_result create_shader(const webgpu_shader_desc* desc,
                                            webgpu_shader* shader) noexcept;
  [[nodiscard]] granit_result destroy_shader(webgpu_shader shader) noexcept;
  [[nodiscard]] granit_result
  create_pipeline_layout(const webgpu_pipeline_layout_desc* desc,
                         webgpu_pipeline_layout* pipeline_layout) noexcept;
  [[nodiscard]] granit_result
  destroy_pipeline_layout(webgpu_pipeline_layout pipeline_layout) noexcept;
  [[nodiscard]] granit_result create_compute_pipeline(const webgpu_compute_pipeline_desc* desc,
                                                      webgpu_compute_pipeline* pipeline) noexcept;
  [[nodiscard]] granit_result destroy_compute_pipeline(webgpu_compute_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result begin_render_pipeline_warmup(const webgpu_render_pipeline_desc* desc,
                                                           webgpu_pipeline_warmup* warmup) noexcept;
  [[nodiscard]] granit_result
  begin_compute_pipeline_warmup(const webgpu_compute_pipeline_desc* desc,
                                webgpu_pipeline_warmup* warmup) noexcept;
  [[nodiscard]] granit_result poll_pipeline_warmup(webgpu_pipeline_warmup warmup) noexcept;
  [[nodiscard]] granit_result destroy_pipeline_warmup(webgpu_pipeline_warmup warmup) noexcept;
  [[nodiscard]] granit_result recorder_begin_compute(webgpu_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_compute_pipeline(webgpu_command_recorder recorder,
                                 webgpu_compute_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_compute_groups(webgpu_command_recorder recorder, webgpu_pipeline_layout layout,
                               std::uint32_t first_group, std::span<const webgpu_bind_group> groups,
                               std::span<const std::uint32_t> dynamic_offsets) noexcept;
  [[nodiscard]] granit_result recorder_dispatch(webgpu_command_recorder recorder, std::uint32_t x,
                                                std::uint32_t y, std::uint32_t z) noexcept;
  [[nodiscard]] granit_result recorder_end_compute(webgpu_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  create_render_pipeline(const webgpu_render_pipeline_desc* desc,
                         webgpu_render_pipeline* render_pipeline) noexcept;
  [[nodiscard]] granit_result destroy_render_pipeline(webgpu_render_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result create_command_recorder(webgpu_command_recorder* recorder) noexcept;
  [[nodiscard]] granit_result destroy_command_recorder(webgpu_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  recorder_copy_buffer_to_texture(webgpu_command_recorder recorder, webgpu_buffer buffer,
                                  webgpu_texture texture, std::uint32_t width, std::uint32_t height,
                                  std::uint32_t bytes_per_row) noexcept;
  [[nodiscard]] granit_result recorder_begin_rendering(
      webgpu_command_recorder recorder, webgpu_texture_view target, webgpu_load_operation load,
      webgpu_store_operation store, const float clear[4], webgpu_texture_view resolve_target = 0,
      webgpu_texture_view depth_target = 0,
      webgpu_load_operation depth_load = GRANIT_WEBGPU_LOAD_OPERATION_CLEAR,
      webgpu_store_operation depth_store = GRANIT_WEBGPU_STORE_OPERATION_DISCARD,
      float clear_depth = 1.0F) noexcept;
  [[nodiscard]] granit_result recorder_bind_pipeline(webgpu_command_recorder recorder,
                                                     webgpu_render_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_graphics_groups(webgpu_command_recorder recorder, webgpu_pipeline_layout layout,
                                std::uint32_t first_group,
                                std::span<const webgpu_bind_group> groups,
                                std::span<const std::uint32_t> dynamic_offsets) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_vertex_buffers(webgpu_command_recorder recorder, std::uint32_t first,
                               std::span<const webgpu_vertex_buffer_binding> bindings) noexcept;
  [[nodiscard]] granit_result recorder_bind_index_buffer(webgpu_command_recorder recorder,
                                                         webgpu_buffer buffer, std::uint64_t offset,
                                                         webgpu_index_format format) noexcept;
  [[nodiscard]] granit_result
  recorder_set_viewports(webgpu_command_recorder recorder, std::uint32_t first,
                         std::span<const webgpu_viewport> viewports) noexcept;
  [[nodiscard]] granit_result
  recorder_set_scissors(webgpu_command_recorder recorder, std::uint32_t first,
                        std::span<const webgpu_scissor> scissors) noexcept;
  [[nodiscard]] granit_result recorder_draw_vertices(webgpu_command_recorder recorder,
                                                     std::uint32_t vertex_count,
                                                     std::uint32_t instance_count,
                                                     std::uint32_t first_vertex,
                                                     std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  recorder_draw_indices(webgpu_command_recorder recorder, std::uint32_t index_count,
                        std::uint32_t instance_count, std::uint32_t first_index,
                        std::int32_t vertex_offset, std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result recorder_end_rendering(webgpu_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  finish_command_recorder(webgpu_command_recorder recorder,
                          webgpu_command_buffer* command_buffer) noexcept;
  [[nodiscard]] granit_result destroy_command_buffer(webgpu_command_buffer command_buffer) noexcept;
  [[nodiscard]] granit_result submit_command_buffer(webgpu_command_buffer command_buffer) noexcept;
  [[nodiscard]] granit_result
  recorder_copy_texture_to_buffer(webgpu_command_recorder recorder, webgpu_texture texture,
                                  webgpu_buffer buffer, std::uint32_t width, std::uint32_t height,
                                  std::uint32_t bytes_per_row) noexcept;
  [[nodiscard]] granit_result
  recorder_copy_buffer(webgpu_command_recorder recorder, webgpu_buffer source,
                       webgpu_buffer destination,
                       std::span<const webgpu_buffer_copy_region> regions) noexcept;
  [[nodiscard]] granit_result
  recorder_copy_buffer_to_texture_v2(webgpu_command_recorder recorder, webgpu_buffer source,
                                     webgpu_texture destination,
                                     const webgpu_texture_buffer_copy& region) noexcept;
  [[nodiscard]] granit_result
  recorder_copy_texture_to_buffer_v2(webgpu_command_recorder recorder, webgpu_texture source,
                                     webgpu_buffer destination,
                                     const webgpu_texture_buffer_copy& region) noexcept;
  [[nodiscard]] granit_result
  recorder_copy_texture(webgpu_command_recorder recorder, webgpu_texture source,
                        webgpu_texture destination,
                        const webgpu_texture_copy_region& region) noexcept;
  [[nodiscard]] granit_result recorder_fill_buffer(webgpu_command_recorder recorder,
                                                   webgpu_buffer buffer, std::uint64_t offset,
                                                   std::uint64_t size,
                                                   std::uint32_t value) noexcept;
  [[nodiscard]] granit_result
  recorder_generate_mipmaps(webgpu_command_recorder recorder, webgpu_texture texture,
                            const webgpu_texture_mipmap_range& range) noexcept;
  [[nodiscard]] granit_result
  create_timestamp_query_pool(std::uint32_t count, webgpu_timestamp_query_pool* pool) noexcept;
  [[nodiscard]] granit_result
  destroy_timestamp_query_pool(webgpu_timestamp_query_pool pool) noexcept;
  [[nodiscard]] granit_result recorder_reset_timestamp_queries(webgpu_command_recorder recorder,
                                                               webgpu_timestamp_query_pool pool,
                                                               std::uint32_t first,
                                                               std::uint32_t count) noexcept;
  [[nodiscard]] granit_result recorder_write_timestamp(webgpu_command_recorder recorder,
                                                       webgpu_timestamp_query_pool pool,
                                                       std::uint32_t index) noexcept;
  [[nodiscard]] granit_result read_timestamp_query_results(webgpu_timestamp_query_pool pool,
                                                           std::uint32_t first,
                                                           std::uint64_t* values,
                                                           std::uint32_t count) noexcept;
  void close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return open_; }
  [[nodiscard]] webgpu_instance_handle instance() const noexcept { return instance_; }

private:
  bool open_{};
  webgpu_instance_handle instance_{};
};

} // namespace granit::detail

#endif
