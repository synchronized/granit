// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_LOADER_H_
#define GRANIT_BACKEND_PLUGIN_LOADER_H_

#include <cstdint>
#include <span>
#include <vector>

#include <granit/core/result.h>

#include "backend/plugin_api.h"
#include "platform/shared_library.h"

namespace granit::detail {

/** 加载并校验 Granit 自有后端插件。 */
class backend_plugin_loader {
public:
  backend_plugin_loader() = default;
  ~backend_plugin_loader();

  backend_plugin_loader(const backend_plugin_loader&) = delete;
  backend_plugin_loader& operator=(const backend_plugin_loader&) = delete;

  /** 加载指定插件；失败后对象保持关闭状态。 */
  [[nodiscard]] granit_result open(const char* library_path,
                                   granit_backend_plugin_kind expected_kind) noexcept;
  /** 校验并接入已静态链接的插件 API；不拥有 API 所在模块。 */
  [[nodiscard]] granit_result open_static(const granit_backend_plugin_api* api,
                                          granit_backend_plugin_kind expected_kind) noexcept;
  [[nodiscard]] granit_result
  create_instance(const granit_backend_plugin_host_api* host,
                  granit_backend_plugin_instance* out_instance) noexcept;
  [[nodiscard]] granit_result destroy_instance(granit_backend_plugin_instance instance) noexcept;
  [[nodiscard]] granit_result
  get_capabilities(granit_backend_plugin_instance instance,
                   granit_backend_plugin_capabilities* capabilities) noexcept;
  [[nodiscard]] granit_result
  get_instance_status(granit_backend_plugin_instance instance,
                      granit_backend_plugin_instance_status* status) noexcept;
  [[nodiscard]] granit_result process_events(granit_backend_plugin_instance instance) noexcept;
  [[nodiscard]] granit_result
  create_win32_surface(granit_backend_plugin_instance instance,
                       const granit_backend_plugin_win32_surface_desc* desc,
                       granit_backend_plugin_surface* surface) noexcept;
  [[nodiscard]] granit_result create_xcb_surface(granit_backend_plugin_instance instance,
                                                 const granit_backend_plugin_xcb_surface_desc* desc,
                                                 granit_backend_plugin_surface* surface) noexcept;
  [[nodiscard]] granit_result
  create_wayland_surface(granit_backend_plugin_instance instance,
                         const granit_backend_plugin_wayland_surface_desc* desc,
                         granit_backend_plugin_surface* surface) noexcept;
  [[nodiscard]] granit_result
  create_canvas_surface(granit_backend_plugin_instance instance,
                        const granit_backend_plugin_canvas_surface_desc* desc,
                        granit_backend_plugin_surface* surface) noexcept;
  [[nodiscard]] granit_result destroy_surface(granit_backend_plugin_instance instance,
                                              granit_backend_plugin_surface surface) noexcept;
  [[nodiscard]] granit_result create_swapchain(granit_backend_plugin_instance instance,
                                               granit_backend_plugin_surface surface,
                                               const granit_backend_plugin_swapchain_desc* desc,
                                               granit_backend_plugin_swapchain* swapchain) noexcept;
  [[nodiscard]] granit_result
  recreate_swapchain(granit_backend_plugin_instance instance,
                     granit_backend_plugin_swapchain swapchain,
                     const granit_backend_plugin_swapchain_desc* desc) noexcept;
  [[nodiscard]] granit_result
  get_swapchain_info(granit_backend_plugin_instance instance,
                     granit_backend_plugin_swapchain swapchain,
                     granit_backend_plugin_swapchain_info* info) noexcept;
  [[nodiscard]] granit_result
  acquire_swapchain(granit_backend_plugin_instance instance,
                    granit_backend_plugin_swapchain swapchain,
                    granit_backend_plugin_acquired_frame* frame) noexcept;
  [[nodiscard]] granit_result present_swapchain(granit_backend_plugin_instance instance,
                                                granit_backend_plugin_swapchain swapchain,
                                                std::uint32_t* needs_recreate) noexcept;
  [[nodiscard]] granit_result cancel_swapchain(granit_backend_plugin_instance instance,
                                               granit_backend_plugin_swapchain swapchain,
                                               std::uint32_t* needs_recreate) noexcept;
  [[nodiscard]] granit_result destroy_swapchain(granit_backend_plugin_instance instance,
                                                granit_backend_plugin_swapchain swapchain) noexcept;
  [[nodiscard]] granit_result create_buffer(granit_backend_plugin_instance instance,
                                            const granit_backend_plugin_buffer_desc* desc,
                                            granit_backend_plugin_buffer* buffer) noexcept;
  [[nodiscard]] granit_result destroy_buffer(granit_backend_plugin_instance instance,
                                             granit_backend_plugin_buffer buffer) noexcept;
  [[nodiscard]] granit_result write_buffer(granit_backend_plugin_instance instance,
                                           granit_backend_plugin_buffer buffer,
                                           std::uint64_t offset, const void* data,
                                           std::uint64_t size) noexcept;
  [[nodiscard]] granit_result read_buffer(granit_backend_plugin_instance instance,
                                          granit_backend_plugin_buffer buffer, std::uint64_t offset,
                                          void* data, std::uint64_t size) noexcept;
  [[nodiscard]] granit_result create_texture(granit_backend_plugin_instance instance,
                                             const granit_backend_plugin_texture_desc* desc,
                                             granit_backend_plugin_texture* texture) noexcept;
  [[nodiscard]] granit_result destroy_texture(granit_backend_plugin_instance instance,
                                              granit_backend_plugin_texture texture) noexcept;
  [[nodiscard]] granit_result
  write_texture(granit_backend_plugin_instance instance, granit_backend_plugin_texture texture,
                const granit_backend_plugin_texture_write_desc* desc, const void* data,
                std::uint64_t size) noexcept;
  [[nodiscard]] granit_result
  create_texture_view(granit_backend_plugin_instance instance,
                      granit_backend_plugin_texture texture,
                      const granit_backend_plugin_texture_view_desc* desc,
                      granit_backend_plugin_texture_view* view) noexcept;
  [[nodiscard]] granit_result
  destroy_texture_view(granit_backend_plugin_instance instance,
                       granit_backend_plugin_texture_view view) noexcept;
  [[nodiscard]] granit_result create_sampler(granit_backend_plugin_instance instance,
                                             const granit_backend_plugin_sampler_desc* desc,
                                             granit_backend_plugin_sampler* sampler) noexcept;
  [[nodiscard]] granit_result destroy_sampler(granit_backend_plugin_instance instance,
                                              granit_backend_plugin_sampler sampler) noexcept;
  [[nodiscard]] granit_result
  create_bind_group_layout(granit_backend_plugin_instance instance,
                           granit_backend_plugin_bind_group_layout* layout) noexcept;
  [[nodiscard]] granit_result
  destroy_bind_group_layout(granit_backend_plugin_instance instance,
                            granit_backend_plugin_bind_group_layout layout) noexcept;
  [[nodiscard]] granit_result
  create_bind_group(granit_backend_plugin_instance instance,
                    const granit_backend_plugin_bind_group_desc* desc,
                    granit_backend_plugin_bind_group* bind_group) noexcept;
  [[nodiscard]] granit_result
  destroy_bind_group(granit_backend_plugin_instance instance,
                     granit_backend_plugin_bind_group bind_group) noexcept;
  [[nodiscard]] granit_result create_shader(granit_backend_plugin_instance instance,
                                            const granit_backend_plugin_shader_desc* desc,
                                            granit_backend_plugin_shader* shader) noexcept;
  [[nodiscard]] granit_result destroy_shader(granit_backend_plugin_instance instance,
                                             granit_backend_plugin_shader shader) noexcept;
  [[nodiscard]] granit_result
  create_pipeline_layout(granit_backend_plugin_instance instance,
                         granit_backend_plugin_bind_group_layout bind_group_layout,
                         granit_backend_plugin_pipeline_layout* pipeline_layout) noexcept;
  [[nodiscard]] granit_result
  destroy_pipeline_layout(granit_backend_plugin_instance instance,
                          granit_backend_plugin_pipeline_layout pipeline_layout) noexcept;
  [[nodiscard]] granit_result
  create_render_pipeline(granit_backend_plugin_instance instance,
                         const granit_backend_plugin_render_pipeline_desc* desc,
                         granit_backend_plugin_render_pipeline* render_pipeline) noexcept;
  [[nodiscard]] granit_result
  destroy_render_pipeline(granit_backend_plugin_instance instance,
                          granit_backend_plugin_render_pipeline render_pipeline) noexcept;
  [[nodiscard]] granit_result
  create_command_recorder(granit_backend_plugin_instance instance,
                          granit_backend_plugin_command_recorder* recorder) noexcept;
  [[nodiscard]] granit_result
  destroy_command_recorder(granit_backend_plugin_instance instance,
                           granit_backend_plugin_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result recorder_copy_buffer_to_texture(
      granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
      granit_backend_plugin_buffer buffer, granit_backend_plugin_texture texture,
      std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept;
  [[nodiscard]] granit_result recorder_begin_rendering(
      granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
      granit_backend_plugin_texture_view target, granit_backend_plugin_load_operation load,
      granit_backend_plugin_store_operation store, const float clear[4]) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_pipeline(granit_backend_plugin_instance instance,
                         granit_backend_plugin_command_recorder recorder,
                         granit_backend_plugin_render_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result recorder_bind_vertex_buffers(
      granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
      std::uint32_t first,
      std::span<const granit_backend_plugin_vertex_buffer_binding> bindings) noexcept;
  [[nodiscard]] granit_result
  recorder_bind_index_buffer(granit_backend_plugin_instance instance,
                             granit_backend_plugin_command_recorder recorder,
                             granit_backend_plugin_buffer buffer, std::uint64_t offset,
                             granit_backend_plugin_index_format format) noexcept;
  [[nodiscard]] granit_result
  recorder_draw_vertices(granit_backend_plugin_instance instance,
                         granit_backend_plugin_command_recorder recorder,
                         std::uint32_t vertex_count, std::uint32_t instance_count,
                         std::uint32_t first_vertex, std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  recorder_draw_indices(granit_backend_plugin_instance instance,
                        granit_backend_plugin_command_recorder recorder, std::uint32_t index_count,
                        std::uint32_t instance_count, std::uint32_t first_index,
                        std::int32_t vertex_offset, std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  recorder_end_rendering(granit_backend_plugin_instance instance,
                         granit_backend_plugin_command_recorder recorder) noexcept;
  [[nodiscard]] granit_result
  finish_command_recorder(granit_backend_plugin_instance instance,
                          granit_backend_plugin_command_recorder recorder,
                          granit_backend_plugin_command_buffer* command_buffer) noexcept;
  [[nodiscard]] granit_result
  destroy_command_buffer(granit_backend_plugin_instance instance,
                         granit_backend_plugin_command_buffer command_buffer) noexcept;
  [[nodiscard]] granit_result
  submit_command_buffer(granit_backend_plugin_instance instance,
                        granit_backend_plugin_command_buffer command_buffer) noexcept;
  [[nodiscard]] granit_result recorder_copy_texture_to_buffer(
      granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
      granit_backend_plugin_texture texture, granit_backend_plugin_buffer buffer,
      std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept;
  void close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return api_ != nullptr; }
  [[nodiscard]] const granit_backend_plugin_api* api() const noexcept { return api_; }

private:
  platform::shared_library library_;
  const granit_backend_plugin_api* api_{nullptr};
  std::vector<granit_backend_plugin_instance> instances_;
};

} // namespace granit::detail

#endif
