// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_REGISTRY_H_
#define GRANIT_RENDERER_RENDERER_REGISTRY_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/frame_context.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/shader.h>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.h>
#include <granit/renderer/timestamp_query.h>
#include <granit/renderer/upload_batch.h>

#include "backend/access.h"
#include "backend/command.h"
#include "backend/compute.h"
#include "backend/interfaces.h"
#include "backend/pipeline.h"
#include "backend/plugin_api.h"
#include "backend/presentation.h"
#include "backend/queue.h"
#include "backend/renderer.h"
#include "backend/rendering.h"
#include "backend/resource_management.h"
#include "backend/resources.h"
#include "backend/retirement.h"
#include "backend/shader.h"
#include "backend/timestamp.h"
#include "backend/transfer.h"
#include "backend/upload.h"
#include "core/handle_table.h"
#include "core/lifecycle_validation.h"
#include "renderer/dynamic_uniform_offsets.h"

namespace granit::detail {

/** 线程安全地管理进程内公开 renderer 句柄。 */
class renderer_registry {
public:
  static renderer_registry& instance();

  [[nodiscard]] granit_result create(std::string_view application_name, bool enable_validation,
                                     std::uint32_t surface_types, std::uint32_t frames_in_flight,
                                     granit_diagnostic_callback diagnostic_callback,
                                     void* diagnostic_user_data, granit_renderer& renderer);
  [[nodiscard]] granit_result register_backend(std::shared_ptr<backend_renderer> backend,
                                               granit_renderer& renderer);
  [[nodiscard]] granit_result destroy(granit_renderer renderer);
  [[nodiscard]] granit_result get_limits(granit_renderer renderer, granit_renderer_limits& limits);
  [[nodiscard]] granit_result get_info(granit_renderer renderer, granit_renderer_info& info);
  [[nodiscard]] granit_result get_resource_stats(granit_renderer renderer,
                                                 granit_renderer_resource_stats& stats);
  [[nodiscard]] granit_result get_status(granit_renderer renderer, granit_renderer_status& status);
  [[nodiscard]] granit_result process_events(granit_renderer renderer);
  [[nodiscard]] granit_result import_pipeline_cache(granit_renderer renderer, const void* data,
                                                    std::uint64_t size);
  [[nodiscard]] granit_result export_pipeline_cache(granit_renderer renderer, void* data,
                                                    std::uint64_t& size);
  [[nodiscard]] granit_result set_object_name(granit_renderer renderer, granit_handle object,
                                              std::string_view name);
  /** 向有效 Renderer 的回调发送公共 API 参数或句柄校验诊断。 */
  void emit_validation_diagnostic(granit_renderer renderer, std::string_view message) noexcept;
  [[nodiscard]] std::shared_ptr<backend_renderer> acquire_backend(granit_renderer renderer);
  [[nodiscard]] std::shared_ptr<const backend_interfaces>
  acquire_backend_interfaces(granit_renderer renderer);
  [[nodiscard]] granit_result create_win32_surface(granit_renderer renderer, void* native_instance,
                                                   void* native_window, granit_surface& surface);
  [[nodiscard]] granit_result create_xcb_surface(granit_renderer renderer, void* connection,
                                                 std::uint32_t window, granit_surface& surface);
  [[nodiscard]] granit_result create_wayland_surface(granit_renderer renderer, void* display,
                                                     void* native_surface, granit_surface& surface);
  [[nodiscard]] granit_result create_canvas_surface(granit_renderer renderer,
                                                    std::string_view selector,
                                                    granit_surface& surface);
  [[nodiscard]] granit_result destroy_surface(granit_renderer renderer, granit_surface surface);
  [[nodiscard]] granit_result create_swapchain(granit_renderer renderer, granit_surface surface,
                                               const backend_swapchain_desc& desc,
                                               granit_swapchain& swapchain);
  [[nodiscard]] granit_result recreate_swapchain(granit_renderer renderer,
                                                 granit_swapchain swapchain,
                                                 const backend_swapchain_desc& desc);
  [[nodiscard]] granit_result get_swapchain_info(granit_renderer renderer,
                                                 granit_swapchain swapchain,
                                                 backend_swapchain_info& info);
  [[nodiscard]] granit_result destroy_swapchain(granit_renderer renderer,
                                                granit_swapchain swapchain);
  [[nodiscard]] granit_result get_swapchain_backbuffer(granit_renderer renderer,
                                                       granit_swapchain swapchain,
                                                       std::uint32_t index, granit_texture& texture,
                                                       granit_texture_view& view);
  [[nodiscard]] granit_result
  acquire_swapchain_frame(granit_renderer renderer, granit_swapchain swapchain, granit_frame& frame,
                          std::uint32_t& image_index, bool& needs_recreate);
  [[nodiscard]] granit_result get_frame_info(granit_renderer renderer, granit_swapchain swapchain,
                                             granit_frame frame, std::uint32_t& frame_slot,
                                             std::uint32_t& frame_slot_count);
  /** 内部渲染子系统查询 Frame 槽；不形成公共 ABI。 */
  [[nodiscard]] granit_result get_frame_slot(granit_renderer renderer, granit_frame frame,
                                             std::uint32_t& frame_slot,
                                             std::uint32_t& frame_slot_count);
  [[nodiscard]] granit_result present_swapchain_frame(granit_renderer renderer,
                                                      granit_swapchain swapchain,
                                                      granit_frame frame, bool& needs_recreate);
  [[nodiscard]] granit_result cancel_swapchain_frame(granit_renderer renderer,
                                                     granit_swapchain swapchain, granit_frame frame,
                                                     bool& needs_recreate);
  [[nodiscard]] granit_result create_buffer(granit_renderer renderer,
                                            const granit_buffer_desc& desc, granit_buffer& buffer);
  [[nodiscard]] granit_result map_buffer(granit_renderer renderer, granit_buffer buffer,
                                         std::uint64_t offset, std::uint64_t size, void*& data);
  [[nodiscard]] granit_result unmap_buffer(granit_renderer renderer, granit_buffer buffer);
  [[nodiscard]] granit_result flush_mapped_buffer(granit_renderer renderer, granit_buffer buffer,
                                                  std::uint64_t offset, std::uint64_t size);
  [[nodiscard]] granit_result get_buffer_desc(granit_renderer renderer, granit_buffer buffer,
                                              granit_buffer_desc& desc);
  [[nodiscard]] granit_result destroy_buffer(granit_renderer renderer, granit_buffer buffer);
  [[nodiscard]] granit_result write_buffer(granit_renderer renderer, granit_buffer buffer,
                                           std::uint64_t offset, const void* data,
                                           std::uint64_t size);
  [[nodiscard]] granit_result create_texture(granit_renderer renderer,
                                             const granit_texture_desc& desc,
                                             granit_texture& texture);
  [[nodiscard]] granit_result write_texture(granit_renderer renderer, granit_texture texture,
                                            const void* data, std::uint64_t size,
                                            const granit_texture_data_layout& layout,
                                            const granit_texture_write_region& region);
  [[nodiscard]] granit_result get_texture_readback_info(granit_renderer renderer,
                                                        granit_texture texture,
                                                        const granit_texture_write_region& region,
                                                        granit_texture_readback_info& info);
  [[nodiscard]] granit_result create_texture_view(granit_renderer renderer, granit_texture texture,
                                                  const granit_texture_view_desc& desc,
                                                  granit_texture_view& view);
  [[nodiscard]] granit_result destroy_texture_view(granit_renderer renderer,
                                                   granit_texture_view view);
  [[nodiscard]] granit_result destroy_texture(granit_renderer renderer, granit_texture texture);
  [[nodiscard]] granit_result create_sampler(granit_renderer renderer,
                                             const granit_sampler_desc& desc,
                                             granit_sampler& sampler);
  [[nodiscard]] granit_result destroy_sampler(granit_renderer renderer, granit_sampler sampler);
  [[nodiscard]] granit_result create_shader(granit_renderer renderer, granit_shader_stage stage,
                                            std::span<const std::uint32_t> code,
                                            std::string_view entry_point, granit_shader& shader);
  [[nodiscard]] granit_result create_shader_from_desc(granit_renderer renderer,
                                                      const granit_shader_desc& desc,
                                                      granit_shader& shader);
  [[nodiscard]] granit_result create_shader_from_wgsl(granit_renderer renderer,
                                                      granit_shader_stage stage,
                                                      std::string_view source,
                                                      std::string_view entry_point,
                                                      granit_shader& shader);
  [[nodiscard]] granit_result destroy_shader(granit_renderer renderer, granit_shader shader);
  [[nodiscard]] granit_result
  create_bind_group_layout(granit_renderer renderer,
                           std::span<const granit_bind_group_layout_entry> entries,
                           granit_bind_group_layout& layout);
  [[nodiscard]] granit_result destroy_bind_group_layout(granit_renderer renderer,
                                                        granit_bind_group_layout layout);
  [[nodiscard]] granit_result create_bind_group(granit_renderer renderer,
                                                const granit_bind_group_desc& desc,
                                                granit_bind_group& bind_group);
  [[nodiscard]] granit_result destroy_bind_group(granit_renderer renderer,
                                                 granit_bind_group bind_group);
  [[nodiscard]] granit_result
  create_pipeline_layout(granit_renderer renderer,
                         std::span<const granit_bind_group_layout> bind_group_layouts,
                         granit_pipeline_layout& layout);
  [[nodiscard]] granit_result destroy_pipeline_layout(granit_renderer renderer,
                                                      granit_pipeline_layout layout);
  [[nodiscard]] granit_result create_graphics_pipeline(granit_renderer renderer,
                                                       const granit_graphics_pipeline_desc& desc,
                                                       granit_graphics_pipeline& pipeline);
  [[nodiscard]] granit_result destroy_graphics_pipeline(granit_renderer renderer,
                                                        granit_graphics_pipeline pipeline);
  [[nodiscard]] granit_result create_compute_pipeline(granit_renderer renderer,
                                                      const granit_compute_pipeline_desc& desc,
                                                      granit_compute_pipeline& pipeline);
  [[nodiscard]] granit_result destroy_compute_pipeline(granit_renderer renderer,
                                                       granit_compute_pipeline pipeline);
  [[nodiscard]] granit_result create_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder& recorder);
  [[nodiscard]] granit_result begin_command_recorder(granit_renderer renderer,
                                                     granit_command_recorder recorder);
  [[nodiscard]] granit_result end_command_recorder(granit_renderer renderer,
                                                   granit_command_recorder recorder);
  [[nodiscard]] granit_result submit_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder);
  [[nodiscard]] granit_result
  submit_command_recorders(granit_renderer renderer,
                           std::span<const granit_command_recorder> recorders);
  [[nodiscard]] granit_result submit_command_recorder_frame(granit_renderer renderer,
                                                            granit_command_recorder recorder,
                                                            granit_frame frame);
  [[nodiscard]] granit_result reset_command_recorder(granit_renderer renderer,
                                                     granit_command_recorder recorder);
  [[nodiscard]] granit_result create_frame_context(granit_renderer renderer,
                                                   granit_frame_context& context);
  [[nodiscard]] granit_result begin_frame_context(granit_renderer renderer,
                                                  granit_frame_context context, granit_frame frame,
                                                  granit_command_recorder& recorder,
                                                  std::uint32_t& frame_slot);
  [[nodiscard]] granit_result
  submit_frame_context(granit_renderer renderer, granit_frame_context context, granit_frame frame);
  [[nodiscard]] granit_result abort_frame_context(granit_renderer renderer,
                                                  granit_frame_context context, granit_frame frame);
  [[nodiscard]] granit_result destroy_frame_context(granit_renderer renderer,
                                                    granit_frame_context context);
  [[nodiscard]] granit_result copy_buffer(granit_renderer renderer,
                                          granit_command_recorder recorder, granit_buffer source,
                                          granit_buffer destination,
                                          std::span<const granit_buffer_copy_region> regions);
  [[nodiscard]] granit_result copy_texture_to_buffer(granit_renderer renderer,
                                                     granit_command_recorder recorder,
                                                     granit_texture source,
                                                     granit_buffer destination,
                                                     const granit_texture_data_layout& layout,
                                                     const granit_texture_write_region& region);
  [[nodiscard]] granit_result copy_buffer_to_texture(granit_renderer renderer,
                                                     granit_command_recorder recorder,
                                                     granit_buffer source,
                                                     granit_texture destination,
                                                     const granit_texture_data_layout& layout,
                                                     const granit_texture_write_region& region);
  [[nodiscard]] granit_result copy_texture(granit_renderer renderer,
                                           granit_command_recorder recorder, granit_texture source,
                                           granit_texture destination,
                                           const granit_texture_copy_region& region);
  [[nodiscard]] granit_result generate_mipmaps(granit_renderer renderer,
                                               granit_command_recorder recorder,
                                               granit_texture texture,
                                               const granit_texture_mipmap_range& range);
  [[nodiscard]] granit_result fill_buffer(granit_renderer renderer,
                                          granit_command_recorder recorder, granit_buffer buffer,
                                          std::uint64_t offset, std::uint64_t size,
                                          std::uint32_t value);
  [[nodiscard]] granit_result bind_graphics_pipeline(granit_renderer renderer,
                                                     granit_command_recorder recorder,
                                                     granit_graphics_pipeline pipeline);
  [[nodiscard]] granit_result bind_graphics_groups(granit_renderer renderer,
                                                   granit_command_recorder recorder,
                                                   granit_pipeline_layout layout,
                                                   std::uint32_t first_group,
                                                   std::span<const granit_bind_group> bind_groups,
                                                   std::span<const std::uint32_t> dynamic_offsets);
  [[nodiscard]] granit_result bind_compute_pipeline(granit_renderer renderer,
                                                    granit_command_recorder recorder,
                                                    granit_compute_pipeline pipeline);
  [[nodiscard]] granit_result bind_compute_groups(granit_renderer renderer,
                                                  granit_command_recorder recorder,
                                                  granit_pipeline_layout layout,
                                                  std::uint32_t first_group,
                                                  std::span<const granit_bind_group> bind_groups,
                                                  std::span<const std::uint32_t> dynamic_offsets);
  [[nodiscard]] granit_result dispatch(granit_renderer renderer, granit_command_recorder recorder,
                                       std::uint32_t group_count_x, std::uint32_t group_count_y,
                                       std::uint32_t group_count_z);
  [[nodiscard]] granit_result set_viewports(granit_renderer renderer,
                                            granit_command_recorder recorder, std::uint32_t first,
                                            std::span<const granit_viewport> viewports);
  [[nodiscard]] granit_result set_scissors(granit_renderer renderer,
                                           granit_command_recorder recorder, std::uint32_t first,
                                           std::span<const granit_scissor> scissors);
  [[nodiscard]] granit_result
  bind_vertex_buffers(granit_renderer renderer, granit_command_recorder recorder,
                      std::uint32_t first, std::span<const granit_vertex_buffer_binding> bindings);
  [[nodiscard]] granit_result bind_index_buffer(granit_renderer renderer,
                                                granit_command_recorder recorder,
                                                granit_buffer buffer, std::uint64_t offset,
                                                granit_index_type type);
  [[nodiscard]] granit_result draw(granit_renderer renderer, granit_command_recorder recorder,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex, std::uint32_t first_instance);
  [[nodiscard]] granit_result draw_indexed(granit_renderer renderer,
                                           granit_command_recorder recorder,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance);
  [[nodiscard]] granit_result begin_rendering(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              const granit_rendering_desc& desc);
  [[nodiscard]] granit_result end_rendering(granit_renderer renderer,
                                            granit_command_recorder recorder);
  [[nodiscard]] granit_result destroy_command_recorder(granit_renderer renderer,
                                                       granit_command_recorder recorder);
  [[nodiscard]] granit_result create_timestamp_query_pool(granit_renderer renderer,
                                                          std::uint32_t query_count,
                                                          granit_timestamp_query_pool& pool);
  [[nodiscard]] granit_result get_timestamp_query_results(granit_renderer renderer,
                                                          granit_timestamp_query_pool pool,
                                                          std::uint32_t first,
                                                          std::span<std::uint64_t> nanoseconds);
  [[nodiscard]] granit_result destroy_timestamp_query_pool(granit_renderer renderer,
                                                           granit_timestamp_query_pool pool);
  [[nodiscard]] granit_result reset_timestamp_queries(granit_renderer renderer,
                                                      granit_command_recorder recorder,
                                                      granit_timestamp_query_pool pool,
                                                      std::uint32_t first, std::uint32_t count);
  [[nodiscard]] granit_result write_timestamp(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              granit_timestamp_query_pool pool,
                                              granit_timestamp_stage stage, std::uint32_t index);
  [[nodiscard]] granit_result create_upload_batch(granit_renderer renderer,
                                                  granit_upload_batch& batch);
  [[nodiscard]] granit_result upload_batch_write_buffer(granit_renderer renderer,
                                                        granit_upload_batch batch,
                                                        granit_buffer buffer, std::uint64_t offset,
                                                        const void* data, std::uint64_t size);
  [[nodiscard]] granit_result upload_batch_write_texture(granit_renderer renderer,
                                                         granit_upload_batch batch,
                                                         granit_texture texture, const void* data,
                                                         std::uint64_t size,
                                                         const granit_texture_data_layout& layout,
                                                         const granit_texture_write_region& region);
  [[nodiscard]] granit_result submit_upload_batch(granit_renderer renderer,
                                                  granit_upload_batch batch);
  [[nodiscard]] granit_result reset_upload_batch(granit_renderer renderer,
                                                 granit_upload_batch batch);
  [[nodiscard]] granit_result destroy_upload_batch(granit_renderer renderer,
                                                   granit_upload_batch batch);

private:
  renderer_registry() = default;

  struct resource_metadata;
  struct retained_resource;
  struct surface_record;
  struct swapchain_record;
  struct buffer_record;
  struct texture_record;
  struct texture_view_record;
  struct sampler_record;
  struct shader_record;
  struct bind_group_layout_record;
  struct pipeline_layout_record;
  struct bind_group_record;
  struct graphics_pipeline_record;
  struct compute_pipeline_record;
  struct command_recorder_record;
  enum class frame_context_slot_state;
  struct frame_context_slot;
  struct frame_context_record;
  struct timestamp_query_pool_record;
  struct frame_record;
  struct upload_entry;
  struct upload_batch_record;

  [[nodiscard]] std::uint32_t allocate_domain() noexcept;
  [[nodiscard]] granit_result
  install_swapchain_backbuffers(granit_swapchain swapchain,
                                const std::shared_ptr<swapchain_record>& record);
  [[nodiscard]] granit_result
  install_swapchain_backbuffers(granit_swapchain swapchain,
                                const std::shared_ptr<swapchain_record>& record,
                                std::vector<backend_swapchain_backbuffer> backbuffers);
  [[nodiscard]] std::shared_ptr<command_recorder_record>
  acquire_command_recorder(granit_renderer renderer, granit_command_recorder recorder);
  void erase_dynamic_backbuffer(swapchain_record& swapchain) noexcept;
  [[nodiscard]] granit_result finish_frame(granit_renderer renderer, granit_swapchain swapchain,
                                           granit_frame frame, bool present, bool& needs_recreate);

  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<backend_renderer>> backend_renderers_;
  std::unordered_map<granit_renderer, std::shared_ptr<const backend_interfaces>>
      backend_interfaces_;

  std::unordered_map<granit_surface, std::shared_ptr<surface_record>> surfaces_;
  std::unordered_map<granit_swapchain, std::shared_ptr<swapchain_record>> swapchains_;
  std::unordered_map<granit_buffer, std::shared_ptr<buffer_record>> buffers_;
  std::unordered_map<granit_texture, std::shared_ptr<texture_record>> textures_;
  std::unordered_map<granit_texture_view, std::shared_ptr<texture_view_record>> texture_views_;
  std::unordered_map<granit_sampler, std::shared_ptr<sampler_record>> samplers_;
  std::unordered_map<granit_shader, std::shared_ptr<shader_record>> shaders_;
  std::unordered_map<granit_pipeline_layout, std::shared_ptr<pipeline_layout_record>>
      pipeline_layouts_;
  std::unordered_map<granit_bind_group_layout, std::shared_ptr<bind_group_layout_record>>
      bind_group_layouts_;
  std::unordered_map<granit_bind_group, std::shared_ptr<bind_group_record>> bind_groups_;
  std::unordered_map<granit_graphics_pipeline, std::shared_ptr<graphics_pipeline_record>>
      graphics_pipelines_;
  std::unordered_map<granit_compute_pipeline, std::shared_ptr<compute_pipeline_record>>
      compute_pipelines_;
  std::unordered_map<granit_command_recorder, std::shared_ptr<command_recorder_record>>
      command_recorders_;
  std::unordered_map<granit_frame_context, std::shared_ptr<frame_context_record>> frame_contexts_;
  std::unordered_map<granit_timestamp_query_pool, std::shared_ptr<timestamp_query_pool_record>>
      timestamp_query_pools_;
  std::unordered_map<granit_frame, std::shared_ptr<frame_record>> frames_;
  std::unordered_map<granit_upload_batch, std::shared_ptr<upload_batch_record>> upload_batches_;
  std::uint32_t next_domain_{1};
  std::uint64_t next_creation_sequence_{1};
};

} // namespace granit::detail

#endif
