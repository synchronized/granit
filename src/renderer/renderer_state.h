// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_STATE_H_
#define GRANIT_RENDERER_RENDERER_STATE_H_

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <source_location>
#include <string_view>
#include <vector>

#include <granit/core/result.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.h>
#include <granit/renderer/timestamp_query.h>

#include "backend/access.h"
#include "backend/binding.h"
#include "backend/capabilities.h"
#include "backend/command.h"
#include "backend/compute.h"
#include "backend/lifecycle.h"
#include "backend/presentation.h"
#include "backend/queue.h"
#include "backend/renderer.h"
#include "backend/rendering.h"
#include "backend/resource_management.h"
#include "backend/retirement.h"
#include "backend/timestamp.h"
#include "backend/transfer.h"
#include "backend/upload.h"
#include "backend/vulkan/command_recorder.h"
#include "backend/vulkan/device.h"
#include "backend/vulkan/frame_context.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/memory_allocator.h"
#include "backend/vulkan/swapchain.h"
#include "backend/vulkan/upload_context.h"
#include "core/device_status.h"
#include "core/diagnostic_sink.h"
#include "core/retirement_queue.h"

namespace granit::detail {

class renderer_state final : public backend_renderer,
                             public backend_resource_renderer,
                             public backend_presentation_renderer,
                             public backend_queue,
                             public backend_command_renderer,
                             public backend_compute_command_renderer,
                             public backend_graphics_command_renderer,
                             public backend_retirement_renderer,
                             public backend_timestamp_renderer,
                             public backend_transfer_command_renderer,
                             public std::enable_shared_from_this<renderer_state> {
public:
  renderer_state() = default;
  ~renderer_state();

  renderer_state(const renderer_state&) = delete;
  renderer_state& operator=(const renderer_state&) = delete;
  renderer_state(renderer_state&&) = delete;
  renderer_state& operator=(renderer_state&&) = delete;

  [[nodiscard]] granit_result initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types,
                                         std::uint32_t frames_in_flight,
                                         granit_diagnostic_callback diagnostic_callback,
                                         void* diagnostic_user_data);
  [[nodiscard]] granit_result import_pipeline_cache(const void* data, std::uint64_t size) noexcept;
  [[nodiscard]] granit_result export_pipeline_cache(void* data, std::uint64_t& size) noexcept;
  [[nodiscard]] granit_result set_object_name(VkObjectType type, std::uint64_t object,
                                              std::string_view name);
  [[nodiscard]] granit_result set_backend_resource_name(backend_resource& resource,
                                                        std::string_view name);

  [[nodiscard]] std::unique_ptr<backend_surface_resource> allocate_surface_resource() override;
  [[nodiscard]] std::unique_ptr<backend_swapchain_resource> allocate_swapchain_resource() override;
  [[nodiscard]] std::unique_ptr<backend_buffer_resource> allocate_buffer_resource() override;
  [[nodiscard]] std::unique_ptr<backend_texture_resource> allocate_texture_resource();
  [[nodiscard]] std::unique_ptr<backend_texture_view_resource> allocate_texture_view_resource();
  [[nodiscard]] std::unique_ptr<backend_sampler_resource> allocate_sampler_resource() override;
  [[nodiscard]] std::unique_ptr<backend_shader_resource> allocate_shader_resource();
  [[nodiscard]] std::unique_ptr<backend_bind_group_layout_resource>
  allocate_bind_group_layout_resource() override;
  [[nodiscard]] std::unique_ptr<backend_bind_group_resource>
  allocate_bind_group_resource() override;
  [[nodiscard]] std::unique_ptr<backend_pipeline_layout_resource>
  allocate_pipeline_layout_resource();
  [[nodiscard]] std::unique_ptr<backend_graphics_pipeline_resource>
  allocate_graphics_pipeline_resource();
  [[nodiscard]] std::unique_ptr<backend_compute_pipeline_resource>
  allocate_compute_pipeline_resource() override;
  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource>
  allocate_command_recorder_resource() override;

  [[nodiscard]] granit_result
  create_win32_surface(void* native_instance, void* native_window,
                       backend_surface_resource& surface) noexcept override;
  [[nodiscard]] granit_result
  create_xcb_surface(void* connection, std::uint32_t window,
                     backend_surface_resource& surface) noexcept override;
  [[nodiscard]] granit_result
  create_wayland_surface(void* display, void* native_surface,
                         backend_surface_resource& surface) noexcept override;
  [[nodiscard]] granit_result
  create_canvas_surface(std::string_view selector,
                        backend_surface_resource& surface) noexcept override;
  void destroy_native_surface(VkSurfaceKHR surface) noexcept;
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
  void destroy_native_swapchain(vulkan_swapchain& swapchain) noexcept;
  [[nodiscard]] granit_result create_native_buffer(const granit_buffer_desc& desc,
                                                   backend_buffer_resource& buffer) noexcept;
  [[nodiscard]] granit_result create_buffer(const granit_buffer_desc& desc,
                                            backend_buffer_resource& buffer) noexcept override {
    return create_native_buffer(desc, buffer);
  }
  void destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept;
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
  [[nodiscard]] granit_result create_native_texture(const granit_texture_desc& desc,
                                                    backend_texture_resource& texture) noexcept;
  [[nodiscard]] bool
  texture_supports_linear_blit(granit_texture_format format) const noexcept override;
  [[nodiscard]] granit_result upload_texture(backend_texture_resource& texture,
                                             granit_texture_format format, const void* data,
                                             std::uint64_t size,
                                             const granit_texture_data_layout& layout,
                                             const granit_texture_write_region& region) noexcept;
  void destroy_native_texture(vulkan_image_allocation& texture) noexcept;
  [[nodiscard]] granit_result create_native_texture_view(
      backend_texture_resource& texture, const granit_texture_desc& texture_desc,
      const granit_texture_view_desc& view_desc, backend_texture_view_resource& view) noexcept;
  void destroy_native_texture_view(VkImageView view) noexcept;
  [[nodiscard]] granit_result create_native_sampler(const granit_sampler_desc& desc,
                                                    backend_sampler_resource& sampler) noexcept;
  [[nodiscard]] granit_result create_sampler(const granit_sampler_desc& desc,
                                             backend_sampler_resource& sampler) noexcept override {
    return create_native_sampler(desc, sampler);
  }
  void destroy_native_sampler(VkSampler sampler) noexcept;
  [[nodiscard]] granit_result create_native_shader(std::span<const std::uint32_t> code,
                                                   backend_shader_resource& shader) noexcept;
  void destroy_native_shader(VkShaderModule shader) noexcept;
  [[nodiscard]] granit_result
  create_native_bind_group_layout(std::span<const granit_bind_group_layout_entry> entries,
                                  backend_bind_group_layout_resource& layout) noexcept;
  [[nodiscard]] granit_result
  create_bind_group_layout(std::span<const granit_bind_group_layout_entry> entries,
                           backend_bind_group_layout_resource& layout) noexcept override {
    return create_native_bind_group_layout(entries, layout);
  }
  void destroy_native_bind_group_layout(VkDescriptorSetLayout layout) noexcept;
  [[nodiscard]] granit_result
  create_native_bind_group(backend_bind_group_layout_resource& layout,
                           std::span<const backend_bind_group_write> writes,
                           backend_bind_group_resource& bind_group) noexcept;
  [[nodiscard]] granit_result
  create_bind_group(backend_bind_group_layout_resource& layout,
                    std::span<const backend_bind_group_write> writes,
                    backend_bind_group_resource& bind_group) noexcept override {
    return create_native_bind_group(layout, writes, bind_group);
  }
  void destroy_native_bind_group(VkDescriptorPool pool) noexcept;
  [[nodiscard]] granit_result create_native_pipeline_layout(
      std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
      backend_pipeline_layout_resource& layout) noexcept;
  void destroy_native_pipeline_layout(VkPipelineLayout layout) noexcept;
  [[nodiscard]] granit_result create_native_graphics_pipeline(
      backend_pipeline_layout_resource& layout, backend_shader_resource& vertex_shader,
      const char* vertex_entry, backend_shader_resource& fragment_shader,
      const char* fragment_entry, std::span<const granit_vertex_buffer_layout> vertex_buffers,
      granit_primitive_state primitive, granit_depth_state depth,
      const granit_depth_bias_state* depth_bias,
      std::span<const granit_color_blend_state> color_blends,
      std::span<const granit_texture_format> color_formats,
      granit_texture_format depth_stencil_format, granit_sample_count sample_count,
      backend_graphics_pipeline_resource& pipeline) noexcept;
  void destroy_native_graphics_pipeline(VkPipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  create_native_compute_pipeline(backend_pipeline_layout_resource& layout,
                                 backend_shader_resource& compute_shader, const char* compute_entry,
                                 backend_compute_pipeline_resource& pipeline) noexcept;
  [[nodiscard]] granit_result
  create_compute_pipeline(backend_pipeline_layout_resource& layout,
                          backend_shader_resource& compute_shader, const char* compute_entry,
                          backend_compute_pipeline_resource& pipeline) noexcept override {
    return create_native_compute_pipeline(layout, compute_shader, compute_entry, pipeline);
  }
  void destroy_native_compute_pipeline(VkPipeline pipeline) noexcept;
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
  copy_buffer(backend_command_recorder_resource& recorder, backend_buffer_resource& source,
              backend_buffer_resource& destination,
              std::span<const granit_buffer_copy_region> regions) override;
  [[nodiscard]] granit_result
  copy_texture_to_buffer(backend_command_recorder_resource& recorder,
                         backend_texture_resource& source, backend_buffer_resource& destination,
                         granit_texture_format format, const granit_texture_data_layout& layout,
                         const granit_texture_write_region& region) override;
  [[nodiscard]] granit_result
  copy_buffer_to_texture(backend_command_recorder_resource& recorder,
                         backend_buffer_resource& source, backend_texture_resource& destination,
                         granit_texture_format format, const granit_texture_data_layout& layout,
                         const granit_texture_write_region& region) override;
  [[nodiscard]] granit_result copy_texture(backend_command_recorder_resource& recorder,
                                           backend_texture_resource& source,
                                           backend_texture_resource& destination,
                                           const granit_texture_copy_region& region) override;
  [[nodiscard]] granit_result generate_mipmaps(backend_command_recorder_resource& recorder,
                                               backend_texture_resource& texture,
                                               const granit_texture_desc& desc,
                                               const granit_texture_mipmap_range& range) override;
  [[nodiscard]] granit_result fill_buffer(backend_command_recorder_resource& recorder,
                                          backend_buffer_resource& buffer, std::uint64_t offset,
                                          std::uint64_t size, std::uint32_t value) override;
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
  [[nodiscard]] granit_result
  bind_compute_pipeline(backend_command_recorder_resource& recorder,
                        backend_compute_pipeline_resource& pipeline) noexcept override;
  [[nodiscard]] granit_result
  bind_compute_groups(backend_command_recorder_resource& recorder,
                      backend_pipeline_layout_resource& layout, std::uint32_t first_group,
                      std::span<backend_bind_group_resource* const> bind_groups,
                      std::span<const std::uint32_t> dynamic_offsets,
                      std::span<const backend_buffer_access> buffer_accesses,
                      std::span<const backend_texture_access> texture_accesses) override;
  [[nodiscard]] granit_result dispatch(backend_command_recorder_resource& recorder,
                                       std::uint32_t group_count_x, std::uint32_t group_count_y,
                                       std::uint32_t group_count_z) noexcept override;
  [[nodiscard]] granit_result
  set_viewports(backend_command_recorder_resource& recorder, std::uint32_t first,
                std::span<const granit_viewport> viewports) noexcept override;
  [[nodiscard]] granit_result
  set_scissors(backend_command_recorder_resource& recorder, std::uint32_t first,
               std::span<const granit_scissor> scissors) noexcept override;
  [[nodiscard]] granit_result bind_vertex_buffers(backend_command_recorder_resource& recorder,
                                                  std::uint32_t first,
                                                  std::span<backend_buffer_resource* const> buffers,
                                                  std::span<const std::uint64_t> offsets) override;
  [[nodiscard]] granit_result bind_index_buffer(backend_command_recorder_resource& recorder,
                                                backend_buffer_resource& buffer,
                                                std::uint64_t offset,
                                                granit_index_type type) override;
  [[nodiscard]] granit_result draw(backend_command_recorder_resource& recorder,
                                   backend_texture_view_resource* target,
                                   backend_graphics_pipeline_resource* pipeline,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex,
                                   std::uint32_t first_instance) noexcept override;
  [[nodiscard]] granit_result draw_indexed(backend_command_recorder_resource& recorder,
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
  [[nodiscard]] granit_result submit_command_recorder(backend_command_recorder_resource& recorder,
                                                      submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result
  submit_command_recorders(std::span<backend_command_recorder_resource* const> recorders,
                           submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result
  acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                          backend_acquired_swapchain_frame& frame) override;
  [[nodiscard]] granit_result submit_swapchain_frame(backend_command_recorder_resource& recorder,
                                                     backend_swapchain_resource& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index,
                                                      bool& needs_recreate) override;
  [[nodiscard]] granit_result cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     bool& needs_recreate) override;
  [[nodiscard]] granit_result
  wait_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result wait_for_all_submissions() noexcept override;
  [[nodiscard]] granit_result wait_for_present_idle() noexcept override;
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
  void retire_resource(submission_serial retire_after, retirement_order order,
                       std::shared_ptr<void> resource) override;
  std::size_t collect_retired() noexcept override;
  std::size_t collect_present_retired() noexcept override { return collect_retired(); }
  std::size_t drain_retired() noexcept;
  void destroy_native_command_recorder(backend_command_recorder_resource& recorder) noexcept;

  void set_domain(std::uint32_t domain) noexcept override { domain_ = domain; }
  [[nodiscard]] std::uint32_t domain() const noexcept override { return domain_; }
  [[nodiscard]] std::size_t frame_slot_count() const noexcept override {
    return frame_slots_.size();
  }
  [[nodiscard]] bool validation_enabled() const noexcept { return validation_enabled_; }
  [[nodiscard]] bool device_lost() const noexcept {
    return lifecycle_.status().state == backend_lifecycle_state::device_lost;
  }
  [[nodiscard]] backend_lifecycle_status lifecycle_status() const noexcept override {
    return lifecycle_.status();
  }
  [[nodiscard]] granit_result process_backend_events() noexcept override {
    return lifecycle_.gate();
  }
  [[nodiscard]] const diagnostic_sink& diagnostics() const noexcept { return diagnostics_; }
  [[nodiscard]] const backend_capabilities& capabilities() const noexcept override {
    return capabilities_;
  }
  [[nodiscard]] const vulkan_instance& instance() const noexcept { return instance_; }
  [[nodiscard]] const vulkan_device& device() const noexcept { return device_; }

private:
  struct frame_slot {
    std::unique_ptr<vulkan_frame_context> context;
    std::unique_ptr<vulkan_command_recorder> preamble;
    std::unique_ptr<vulkan_command_recorder> postamble;
    std::vector<std::unique_ptr<vulkan_command_recorder>> batch_preambles;
    std::vector<vulkan_command_recorder*> recorders;
    submission_serial serial{};
    bool acquired{};
    bool awaiting_present{};
  };

  struct upload_slot {
    std::unique_ptr<vulkan_upload_context> context;
    bool acquired{};
  };

  [[nodiscard]] std::size_t acquire_upload_slot();
  void release_upload_slot(std::size_t index) noexcept;

  [[nodiscard]] granit_result complete_frame_slot(frame_slot& slot) noexcept;
  [[nodiscard]] granit_result observe_device_result(
      granit_result result,
      const std::source_location& location = std::source_location::current()) noexcept;

  std::uint32_t domain_{};
  std::uint32_t surface_types_{};
  bool validation_enabled_{};
  diagnostic_sink diagnostics_;
  device_status device_status_;
  backend_lifecycle lifecycle_;
  backend_capabilities capabilities_;
  std::mutex resource_mutex_;
  std::mutex pipeline_cache_mutex_;
  std::mutex queue_mutex_;
  std::mutex upload_mutex_;
  std::condition_variable upload_available_;
  vulkan_instance instance_;
  vulkan_device device_;
  vulkan_memory_allocator memory_allocator_;
  VkPipelineCache pipeline_cache_{VK_NULL_HANDLE};
  std::vector<frame_slot> frame_slots_;
  std::vector<upload_slot> upload_slots_;
  std::size_t next_frame_slot_{};
  submission_serials submission_serials_;
  std::mutex retirement_mutex_;
  retirement_queue retirement_queue_;
  std::vector<vulkan_image_access> image_states_;
};

} // namespace granit::detail

#endif
