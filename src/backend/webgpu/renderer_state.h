// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RENDERER_STATE_H_
#define GRANIT_BACKEND_WEBGPU_RENDERER_STATE_H_

#include <memory>

#include <granit/core/diagnostic.h>

#include "backend/contracts/capabilities.h"
#include "backend/contracts/command.h"
#include "backend/contracts/compute.h"
#include "backend/contracts/lifecycle.h"
#include "backend/contracts/pipeline.h"
#include "backend/contracts/queue.h"
#include "backend/contracts/renderer.h"
#include "backend/contracts/rendering.h"
#include "backend/contracts/resource_management.h"
#include "backend/contracts/retirement.h"
#include "backend/contracts/shader.h"
#include "backend/contracts/timestamp.h"
#include "backend/contracts/transfer.h"
#include "backend/webgpu/commands.h"
#include "backend/webgpu/device.h"
#include "backend/webgpu/pipelines.h"
#include "backend/webgpu/presentation.h"
#include "backend/webgpu/resources.h"

namespace granit::detail {

/** 实现 WebGPU HAL 契约并管理异步生命周期、能力快照和设备资源。 */
class webgpu_renderer_state final : public backend_renderer,
                                    public backend_presentation_renderer,
                                    public backend_queue,
                                    public backend_command_renderer,
                                    public backend_graphics_command_renderer,
                                    public backend_compute_command_renderer,
                                    public backend_resource_renderer,
                                    public backend_transfer_command_renderer,
                                    public backend_retirement_renderer,
                                    public backend_shader_renderer,
                                    public backend_pipeline_layout_renderer,
                                    public backend_pipeline_renderer,
                                    public backend_pipeline_warmup_renderer,
                                    public backend_timestamp_renderer {
public:
  webgpu_renderer_state() = default;
  ~webgpu_renderer_state();

  webgpu_renderer_state(const webgpu_renderer_state&) = delete;
  webgpu_renderer_state& operator=(const webgpu_renderer_state&) = delete;

  [[nodiscard]] granit_result initialize_static(std::uint32_t surface_types,
                                                granit_diagnostic_callback diagnostic_callback,
                                                void* diagnostic_user_data) noexcept;
  [[nodiscard]] granit_result process_backend_events() noexcept override;
  [[nodiscard]] granit_renderer_backend backend() const noexcept override {
    return GRANIT_RENDERER_BACKEND_WEBGPU;
  }
  [[nodiscard]] std::string_view adapter_name() const noexcept override { return {}; }
  [[nodiscard]] std::uint32_t adapter_vendor_id() const noexcept override { return 0; }
  [[nodiscard]] std::uint32_t adapter_device_id() const noexcept override { return 0; }

  [[nodiscard]] backend_lifecycle_status lifecycle_status() const noexcept override;
  [[nodiscard]] const backend_capabilities& capabilities() const noexcept override {
    return capabilities_;
  }
  [[nodiscard]] backend_texture_format_capabilities
  texture_format_capabilities(granit_texture_format format) const noexcept override;
  [[nodiscard]] std::uint32_t domain() const noexcept override { return domain_; }
  void set_domain(std::uint32_t domain) noexcept override { domain_ = domain; }

  [[nodiscard]] std::unique_ptr<backend_buffer_resource> allocate_buffer_resource() override;
  [[nodiscard]] granit_result create_buffer(const granit_buffer_desc& desc,
                                            backend_buffer_resource& buffer) noexcept override;
  [[nodiscard]] void* mapped_buffer_data(backend_buffer_resource& buffer) noexcept override;
  [[nodiscard]] granit_result flush_buffer(backend_buffer_resource& buffer, std::uint64_t offset,
                                           std::uint64_t size) noexcept override;
  [[nodiscard]] granit_result invalidate_buffer(backend_buffer_resource& buffer,
                                                std::uint64_t offset,
                                                std::uint64_t size) noexcept override;
  [[nodiscard]] granit_result upload_buffer(backend_buffer_resource& buffer, std::uint64_t offset,
                                            const void* data, std::uint64_t size) noexcept override;
  [[nodiscard]] granit_result
  upload_batch(std::span<const backend_upload_operation> uploads) noexcept override;
  [[nodiscard]] granit_result
  upload_batch_async(std::span<const backend_upload_operation> uploads,
                     std::unique_ptr<backend_upload_completion>& completion) noexcept override;
  [[nodiscard]] granit_result
  readback_batch_async(std::span<const backend_readback_operation> readbacks,
                       granit_readback_layout layout, std::uint64_t max_result_bytes,
                       std::unique_ptr<backend_readback_completion>& completion) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_texture_resource> allocate_texture_resource() override;
  [[nodiscard]] granit_result create_texture(const granit_texture_desc&,
                                             backend_texture_resource&) noexcept override;
  [[nodiscard]] granit_result upload_texture(backend_texture_resource&, granit_texture_format,
                                             const void*, std::uint64_t,
                                             const granit_texture_data_layout&,
                                             const granit_texture_write_region&) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_texture_view_resource>
  allocate_texture_view_resource() override;
  [[nodiscard]] granit_result create_texture_view(backend_texture_resource&,
                                                  const granit_texture_desc&,
                                                  const granit_texture_view_desc&,
                                                  backend_texture_view_resource&) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_sampler_resource> allocate_sampler_resource() override;
  [[nodiscard]] granit_result create_sampler(const granit_sampler_desc&,
                                             backend_sampler_resource&) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_bind_group_layout_resource>
  allocate_bind_group_layout_resource() override;
  [[nodiscard]] granit_result
  create_bind_group_layout(std::span<const granit_bind_group_layout_entry>,
                           backend_bind_group_layout_resource&) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_bind_group_resource>
  allocate_bind_group_resource() override;
  [[nodiscard]] granit_result create_bind_group(backend_bind_group_layout_resource&,
                                                std::span<const backend_bind_group_write>,
                                                backend_bind_group_resource&) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_compute_pipeline_resource>
  allocate_compute_pipeline_resource() override;
  [[nodiscard]] granit_result
  create_compute_pipeline(backend_pipeline_layout_resource&, backend_shader_resource&, const char*,
                          backend_compute_pipeline_resource&) noexcept override;
  [[nodiscard]] granit_result warmup_graphics_pipeline_async(
      const backend_graphics_pipeline_create_info& info,
      std::unique_ptr<backend_pipeline_warmup_completion>& completion) noexcept override;
  [[nodiscard]] granit_result warmup_compute_pipeline_async(
      backend_pipeline_layout_resource& layout, backend_shader_resource& shader,
      const char* entry_point,
      std::unique_ptr<backend_pipeline_warmup_completion>& completion) noexcept override;
  [[nodiscard]] granit_result
  bind_compute_pipeline(backend_command_recorder_resource&,
                        backend_compute_pipeline_resource&) noexcept override;
  [[nodiscard]] granit_result bind_compute_groups(backend_command_recorder_resource&,
                                                  backend_pipeline_layout_resource&, std::uint32_t,
                                                  std::span<backend_bind_group_resource* const>,
                                                  std::span<const std::uint32_t>,
                                                  std::span<const backend_buffer_access>,
                                                  std::span<const backend_texture_access>) override;
  [[nodiscard]] granit_result dispatch(backend_command_recorder_resource&, std::uint32_t,
                                       std::uint32_t, std::uint32_t) noexcept override;

  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource>
  allocate_command_recorder_resource() override;
  [[nodiscard]] granit_result
  create_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  begin_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  end_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  reset_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  discard_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] bool
  command_recorder_is_recording(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  bind_graphics_pipeline(backend_command_recorder_resource& recorder,
                         backend_graphics_pipeline_resource& pipeline) noexcept override;
  [[nodiscard]] granit_result
  bind_graphics_groups(backend_command_recorder_resource& recorder,
                       backend_pipeline_layout_resource& layout, std::uint32_t first_group,
                       std::span<backend_bind_group_resource* const> bind_groups,
                       std::span<const std::uint32_t> dynamic_offsets,
                       std::span<const backend_buffer_access> buffer_accesses,
                       std::span<const backend_texture_access> texture_accesses) override;
  [[nodiscard]] granit_result bind_vertex_buffers(backend_command_recorder_resource& recorder,
                                                  std::uint32_t first,
                                                  std::span<backend_buffer_resource* const> buffers,
                                                  std::span<const std::uint64_t> offsets) override;
  [[nodiscard]] granit_result bind_index_buffer(backend_command_recorder_resource& recorder,
                                                backend_buffer_resource& buffer,
                                                std::uint64_t offset,
                                                granit_index_type type) override;
  [[nodiscard]] granit_result
  set_viewports(backend_command_recorder_resource& recorder, std::uint32_t first,
                std::span<const granit_viewport> viewports) noexcept override;
  [[nodiscard]] granit_result
  set_scissors(backend_command_recorder_resource& recorder, std::uint32_t first,
               std::span<const granit_scissor> scissors) noexcept override;
  [[nodiscard]] granit_result copy_buffer(backend_command_recorder_resource&,
                                          backend_buffer_resource&, backend_buffer_resource&,
                                          std::span<const granit_buffer_copy_region>) override;
  [[nodiscard]] granit_result
  copy_texture_to_buffer(backend_command_recorder_resource& recorder,
                         backend_texture_resource& source, backend_buffer_resource& destination,
                         granit_texture_format format, const granit_texture_data_layout& layout,
                         const granit_texture_write_region& region) override;
  [[nodiscard]] granit_result copy_buffer_to_texture(backend_command_recorder_resource&,
                                                     backend_buffer_resource&,
                                                     backend_texture_resource&,
                                                     granit_texture_format,
                                                     const granit_texture_data_layout&,
                                                     const granit_texture_write_region&) override;
  [[nodiscard]] granit_result copy_texture(backend_command_recorder_resource&,
                                           backend_texture_resource&, backend_texture_resource&,
                                           const granit_texture_copy_region&) override;
  [[nodiscard]] bool texture_supports_linear_blit(granit_texture_format) const noexcept override;
  [[nodiscard]] granit_result generate_mipmaps(backend_command_recorder_resource&,
                                               backend_texture_resource&,
                                               const granit_texture_desc&,
                                               const granit_texture_mipmap_range&) override;
  [[nodiscard]] granit_result fill_buffer(backend_command_recorder_resource&,
                                          backend_buffer_resource&, std::uint64_t, std::uint64_t,
                                          std::uint32_t) override;
  [[nodiscard]] granit_result draw(backend_command_recorder_resource& recorder,
                                   backend_texture_view_resource* target,
                                   backend_graphics_pipeline_resource* pipeline,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex,
                                   std::uint32_t first_instance) noexcept override;
  [[nodiscard]] granit_result draw_indexed(backend_command_recorder_resource& recorder,
                                           backend_texture_view_resource* target,
                                           backend_graphics_pipeline_resource* pipeline,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance) noexcept override;
  [[nodiscard]] granit_result
  begin_rendering(backend_command_recorder_resource& recorder, granit_rendering_area area,
                  std::span<const backend_color_attachment> color_attachments,
                  const backend_depth_stencil_attachment* depth_stencil_attachment,
                  std::uint32_t layer_count) override;
  [[nodiscard]] granit_result
  end_rendering(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_shader_resource> allocate_shader_resource() override;
  [[nodiscard]] granit_result create_shader(backend_shader_resource& shader,
                                            granit_shader_stage stage,
                                            granit_shader_code_format code_format,
                                            std::span<const std::byte> code,
                                            std::string_view entry_point) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_pipeline_layout_resource>
  allocate_pipeline_layout_resource() override;
  [[nodiscard]] granit_result
  create_pipeline_layout(std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
                         backend_pipeline_layout_resource& layout) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_graphics_pipeline_resource>
  allocate_graphics_pipeline_resource() override;
  [[nodiscard]] granit_result
  validate_graphics_pipeline(const granit_graphics_pipeline_desc& desc) const noexcept override;
  [[nodiscard]] granit_result
  create_graphics_pipeline(const backend_graphics_pipeline_create_info& info,
                           backend_graphics_pipeline_resource& pipeline) noexcept override;

  [[nodiscard]] granit_result create_timestamp_query_pool(
      std::uint32_t query_count,
      std::unique_ptr<backend_timestamp_query_pool_resource>& pool) noexcept override;
  [[nodiscard]] granit_result
  read_timestamp_query_results(backend_timestamp_query_pool_resource& pool, std::uint32_t first,
                               std::span<std::uint64_t> values) noexcept override;
  [[nodiscard]] granit_result reset_timestamp_queries(backend_command_recorder_resource& recorder,
                                                      backend_timestamp_query_pool_resource& pool,
                                                      std::uint32_t first,
                                                      std::uint32_t count) noexcept override;
  [[nodiscard]] granit_result write_timestamp(backend_command_recorder_resource& recorder,
                                              backend_timestamp_query_pool_resource& pool,
                                              granit_timestamp_stage stage,
                                              std::uint32_t index) noexcept override;
  [[nodiscard]] granit_result
  set_timestamp_query_pool_name(backend_timestamp_query_pool_resource& pool,
                                std::string_view name) noexcept override;

  [[nodiscard]] std::unique_ptr<backend_surface_resource> allocate_surface_resource() override;
  [[nodiscard]] std::unique_ptr<backend_swapchain_resource> allocate_swapchain_resource() override;
  [[nodiscard]] granit_result create_win32_surface(void*, void*,
                                                   backend_surface_resource&) noexcept override;
  [[nodiscard]] granit_result create_xcb_surface(void*, std::uint32_t,
                                                 backend_surface_resource&) noexcept override;
  [[nodiscard]] granit_result create_wayland_surface(void*, void*,
                                                     backend_surface_resource&) noexcept override;
  [[nodiscard]] granit_result
  create_canvas_surface(std::string_view selector,
                        backend_surface_resource& surface) noexcept override;
  [[nodiscard]] granit_result create_swapchain(backend_surface_resource& surface,
                                               const backend_swapchain_desc& desc,
                                               backend_swapchain_resource& swapchain) override;
  [[nodiscard]] granit_result recreate_swapchain(backend_surface_resource& surface,
                                                 const backend_swapchain_desc& desc,
                                                 backend_swapchain_resource& swapchain) override;
  [[nodiscard]] backend_swapchain_info
  get_swapchain_info(backend_swapchain_resource& swapchain) noexcept override;
  [[nodiscard]] granit_result
  get_swapchain_backbuffers(backend_swapchain_resource& swapchain,
                            std::vector<backend_swapchain_backbuffer>& backbuffers) override;
  [[nodiscard]] granit_result
  prepare_swapchain_backbuffer(backend_swapchain_backbuffer& backbuffer) override;
  [[nodiscard]] granit_result
  acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                          backend_acquired_swapchain_frame& frame) override;
  [[nodiscard]] granit_result present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index,
                                                      bool& needs_recreate) override;
  [[nodiscard]] granit_result cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     bool& needs_recreate) override;
  [[nodiscard]] granit_result wait_for_present_idle() noexcept override;
  std::size_t collect_present_retired() noexcept override;
  [[nodiscard]] std::size_t frame_slot_count() const noexcept override;
  [[nodiscard]] granit_result submit_command_recorder(backend_command_recorder_resource& recorder,
                                                      submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result
  submit_command_recorders(std::span<backend_command_recorder_resource* const> recorders,
                           submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result
  wait_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result wait_for_all_submissions() noexcept override;
  void retire_resource(submission_serial retire_after, retirement_order order,
                       std::shared_ptr<void> resource) override;
  [[nodiscard]] std::size_t collect_retired() noexcept override;
  [[nodiscard]] std::size_t pending_retirement_count() const noexcept override;
  [[nodiscard]] granit_result submit_swapchain_frame(backend_command_recorder_resource& recorder,
                                                     backend_swapchain_resource& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     submission_serial& submitted_serial) override;

private:
  static void* allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept;
  static void deallocate(void* memory, std::uint64_t size, std::uint64_t alignment, void*) noexcept;
  static void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
                       const char* message, std::uint32_t message_length, void* user_data) noexcept;
  [[nodiscard]] granit_result refresh_state() noexcept;
  [[nodiscard]] granit_result finish_initialization() noexcept;
  [[nodiscard]] std::unique_ptr<backend_surface_resource> presentation_allocate_surface();
  [[nodiscard]] std::unique_ptr<backend_swapchain_resource> presentation_allocate_swapchain();
  [[nodiscard]] granit_result presentation_create_win32_surface(backend_surface_resource& resource,
                                                                void* instance,
                                                                void* window) noexcept;
  [[nodiscard]] granit_result presentation_create_xcb_surface(backend_surface_resource& resource,
                                                              void* connection,
                                                              std::uint32_t window) noexcept;
  [[nodiscard]] granit_result
  presentation_create_wayland_surface(backend_surface_resource& resource, void* display,
                                      void* surface) noexcept;
  [[nodiscard]] granit_result
  presentation_create_canvas_surface(backend_surface_resource& resource, const char* selector,
                                     std::uint32_t selector_length) noexcept;
  [[nodiscard]] granit_result
  presentation_create_swapchain(backend_surface_resource& surface,
                                const backend_swapchain_desc& desc,
                                backend_swapchain_resource& swapchain) noexcept;
  [[nodiscard]] granit_result
  presentation_recreate_swapchain(backend_swapchain_resource& swapchain,
                                  const backend_swapchain_desc& desc) noexcept;
  [[nodiscard]] granit_result
  presentation_get_swapchain_info(backend_swapchain_resource& swapchain,
                                  backend_swapchain_info& info) noexcept;
  [[nodiscard]] granit_result
  presentation_acquire_swapchain(backend_swapchain_resource& swapchain,
                                 backend_acquired_swapchain_frame& frame) noexcept;
  [[nodiscard]] granit_result presentation_present_swapchain(backend_swapchain_resource& swapchain,
                                                             bool& needs_recreate) noexcept;
  [[nodiscard]] granit_result presentation_cancel_swapchain(backend_swapchain_resource& swapchain,
                                                            bool& needs_recreate) noexcept;
  [[nodiscard]] webgpu_texture_view
  presentation_native_view(backend_texture_view_resource& view) noexcept;
  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource> command_allocate_recorder();
  [[nodiscard]] granit_result command_begin(backend_command_recorder_resource& resource) noexcept;
  [[nodiscard]] granit_result
  command_begin_rendering(backend_command_recorder_resource& resource, webgpu_texture_view target,
                          webgpu_texture_view resolve_target, webgpu_load_operation load,
                          webgpu_store_operation store, const float clear[4],
                          webgpu_texture_view depth_target, webgpu_load_operation depth_load,
                          webgpu_store_operation depth_store, float clear_depth) noexcept;
  [[nodiscard]] granit_result command_bind_pipeline(backend_command_recorder_resource& resource,
                                                    webgpu_render_pipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  command_bind_graphics_groups(backend_command_recorder_resource& resource,
                               webgpu_pipeline_layout layout, std::uint32_t first_group,
                               std::span<const webgpu_bind_group> groups,
                               std::span<const std::uint32_t> dynamic_offsets) noexcept;
  [[nodiscard]] granit_result command_begin_compute(backend_command_recorder_resource&) noexcept;
  [[nodiscard]] granit_result command_bind_compute_pipeline(backend_command_recorder_resource&,
                                                            webgpu_compute_pipeline) noexcept;
  [[nodiscard]] granit_result command_bind_compute_groups(backend_command_recorder_resource&,
                                                          webgpu_pipeline_layout, std::uint32_t,
                                                          std::span<const webgpu_bind_group>,
                                                          std::span<const std::uint32_t>) noexcept;
  [[nodiscard]] granit_result command_dispatch(backend_command_recorder_resource&, std::uint32_t,
                                               std::uint32_t, std::uint32_t) noexcept;
  [[nodiscard]] granit_result command_end_compute(backend_command_recorder_resource&) noexcept;
  [[nodiscard]] granit_result
  command_bind_vertex_buffers(backend_command_recorder_resource& resource, std::uint32_t first,
                              std::span<const webgpu_vertex_buffer_binding> bindings) noexcept;
  [[nodiscard]] granit_result command_bind_index_buffer(backend_command_recorder_resource& resource,
                                                        webgpu_buffer buffer, std::uint64_t offset,
                                                        webgpu_index_format format) noexcept;
  [[nodiscard]] granit_result
  command_set_viewports(backend_command_recorder_resource& resource, std::uint32_t first,
                        std::span<const webgpu_viewport> viewports) noexcept;
  [[nodiscard]] granit_result
  command_set_scissors(backend_command_recorder_resource& resource, std::uint32_t first,
                       std::span<const webgpu_scissor> scissors) noexcept;
  [[nodiscard]] granit_result
  command_copy_texture_to_buffer(backend_command_recorder_resource& resource,
                                 webgpu_texture texture, webgpu_buffer buffer, std::uint32_t width,
                                 std::uint32_t height, std::uint32_t bytes_per_row) noexcept;
  [[nodiscard]] granit_result
  command_copy_buffer(backend_command_recorder_resource& resource, webgpu_buffer source,
                      webgpu_buffer destination,
                      std::span<const webgpu_buffer_copy_region> regions) noexcept;
  [[nodiscard]] granit_result
  command_copy_buffer_to_texture(backend_command_recorder_resource& resource, webgpu_buffer source,
                                 webgpu_texture destination,
                                 const webgpu_texture_buffer_copy& region) noexcept;
  [[nodiscard]] granit_result
  command_copy_texture_to_buffer(backend_command_recorder_resource& resource, webgpu_texture source,
                                 webgpu_buffer destination,
                                 const webgpu_texture_buffer_copy& region) noexcept;
  [[nodiscard]] granit_result
  command_copy_texture(backend_command_recorder_resource& resource, webgpu_texture source,
                       webgpu_texture destination,
                       const webgpu_texture_copy_region& region) noexcept;
  [[nodiscard]] granit_result command_fill_buffer(backend_command_recorder_resource& resource,
                                                  webgpu_buffer buffer, std::uint64_t offset,
                                                  std::uint64_t size, std::uint32_t value) noexcept;
  [[nodiscard]] granit_result
  command_generate_mipmaps(backend_command_recorder_resource& resource, webgpu_texture texture,
                           const webgpu_texture_mipmap_range& range) noexcept;
  [[nodiscard]] granit_result command_draw(backend_command_recorder_resource& resource,
                                           std::uint32_t vertex_count, std::uint32_t instance_count,
                                           std::uint32_t first_vertex,
                                           std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  command_draw_indexed(backend_command_recorder_resource& resource, std::uint32_t index_count,
                       std::uint32_t instance_count, std::uint32_t first_index,
                       std::int32_t vertex_offset, std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  command_end_rendering(backend_command_recorder_resource& resource) noexcept;
  [[nodiscard]] bool command_is_recording(backend_command_recorder_resource& resource) noexcept;
  [[nodiscard]] granit_result command_end(backend_command_recorder_resource& resource) noexcept;
  [[nodiscard]] granit_result command_submit(backend_command_recorder_resource& resource) noexcept;
  [[nodiscard]] granit_result command_reset(backend_command_recorder_resource& resource) noexcept;
  [[nodiscard]] webgpu_command_recorder
  command_native_recorder(backend_command_recorder_resource& resource) noexcept;

  [[nodiscard]] webgpu_shader native_shader(backend_shader_resource& resource) const noexcept;
  [[nodiscard]] webgpu_buffer native_buffer(backend_buffer_resource& resource) const noexcept;
  [[nodiscard]] webgpu_texture native_texture(backend_texture_resource& resource) const noexcept;
  [[nodiscard]] webgpu_texture_view
  native_texture_view(backend_texture_view_resource& resource) const noexcept;
  [[nodiscard]] webgpu_bind_group_layout
  native_bind_group_layout(backend_bind_group_layout_resource& resource) const noexcept;
  [[nodiscard]] webgpu_bind_group
  native_bind_group(backend_bind_group_resource& resource) const noexcept;
  [[nodiscard]] std::unique_ptr<backend_pipeline_layout_resource> allocate_pipeline_layout();
  [[nodiscard]] std::unique_ptr<backend_graphics_pipeline_resource> allocate_graphics_pipeline();
  [[nodiscard]] std::unique_ptr<backend_compute_pipeline_resource> allocate_compute_pipeline();
  [[nodiscard]] granit_result
  create_pipeline_layout(std::span<const webgpu_bind_group_layout> layouts,
                         backend_pipeline_layout_resource& resource) noexcept;
  [[nodiscard]] webgpu_pipeline_layout
  native_pipeline_layout(backend_pipeline_layout_resource& resource) const noexcept;
  [[nodiscard]] granit_result create_compute_pipeline(backend_compute_pipeline_resource& resource,
                                                      webgpu_pipeline_layout layout,
                                                      webgpu_shader shader) noexcept;
  [[nodiscard]] granit_result
  begin_compute_pipeline_warmup(webgpu_pipeline_layout layout, webgpu_shader shader,
                                webgpu_pipeline_warmup& warmup) noexcept;
  [[nodiscard]] webgpu_compute_pipeline
  native_compute_pipeline(backend_compute_pipeline_resource& resource) const noexcept;
  [[nodiscard]] granit_result create_graphics_pipeline(
      backend_graphics_pipeline_resource& resource, backend_pipeline_layout_resource& layout,
      webgpu_shader vertex_shader, webgpu_shader fragment_shader,
      std::span<const granit_vertex_buffer_layout> vertex_buffers,
      granit_texture_format color_format, granit_texture_format depth_stencil_format,
      granit_sample_count sample_count, const granit_primitive_state& primitive,
      const granit_depth_state& depth, const granit_depth_bias_state* depth_bias,
      const granit_color_blend_state& color_blend) noexcept;
  [[nodiscard]] granit_result begin_graphics_pipeline_warmup(
      backend_pipeline_layout_resource& layout, webgpu_shader vertex_shader,
      webgpu_shader fragment_shader, std::span<const granit_vertex_buffer_layout> vertex_buffers,
      granit_texture_format color_format, granit_texture_format depth_stencil_format,
      granit_sample_count sample_count, const granit_primitive_state& primitive,
      const granit_depth_state& depth, const granit_depth_bias_state* depth_bias,
      const granit_color_blend_state& color_blend, webgpu_pipeline_warmup& warmup) noexcept;
  [[nodiscard]] granit_result create_graphics_pipeline_impl(
      backend_graphics_pipeline_resource* resource, backend_pipeline_layout_resource& layout,
      webgpu_shader vertex_shader, webgpu_shader fragment_shader,
      std::span<const granit_vertex_buffer_layout> vertex_buffers,
      granit_texture_format color_format, granit_texture_format depth_stencil_format,
      granit_sample_count sample_count, const granit_primitive_state& primitive,
      const granit_depth_state& depth, const granit_depth_bias_state* depth_bias,
      const granit_color_blend_state& color_blend, webgpu_pipeline_warmup* warmup) noexcept;
  [[nodiscard]] webgpu_render_pipeline
  native_graphics_pipeline(backend_graphics_pipeline_resource& resource) const noexcept;
  [[nodiscard]] webgpu_timestamp_query_pool
  native_timestamp_query_pool(backend_timestamp_query_pool_resource& resource) const noexcept;

  webgpu_device device_;
  webgpu_instance_handle instance_{};
  granit_diagnostic_callback diagnostic_callback_{};
  void* diagnostic_user_data_{};
  backend_lifecycle_status lifecycle_{};
  backend_capabilities capabilities_{};
  std::uint32_t surface_types_{};
  std::uint32_t device_surface_types_{};
  std::uint32_t domain_{};
  submission_serial next_submission_serial_{1};
  std::shared_ptr<webgpu_presentation_owner> presentation_owner_;
  std::shared_ptr<webgpu_resource_owner> resource_owner_;
  std::shared_ptr<webgpu_pipeline_owner> pipeline_owner_;
  std::shared_ptr<webgpu_command_owner> command_owner_;
};

} // namespace granit::detail

#endif
