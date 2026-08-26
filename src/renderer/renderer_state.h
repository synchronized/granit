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
#include <granit/renderer/pipeline.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.h>

#include "backend/capabilities.h"
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

struct vulkan_bind_group_write {
  std::uint32_t binding{};
  std::uint32_t array_element{};
  VkDescriptorType type{};
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceSize offset{};
  VkDeviceSize range{};
  VkImageView image_view{VK_NULL_HANDLE};
  VkSampler sampler{VK_NULL_HANDLE};
};

class renderer_state {
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

  [[nodiscard]] granit_result create_win32_surface(void* native_instance, void* native_window,
                                                   VkSurfaceKHR& surface) noexcept;
  [[nodiscard]] granit_result create_xcb_surface(void* connection, std::uint32_t window,
                                                 VkSurfaceKHR& surface) noexcept;
  [[nodiscard]] granit_result create_wayland_surface(void* display, void* native_surface,
                                                     VkSurfaceKHR& surface) noexcept;
  void destroy_native_surface(VkSurfaceKHR surface) noexcept;
  [[nodiscard]] granit_result create_swapchain(VkSurfaceKHR surface,
                                               const vulkan_swapchain_desc& desc,
                                               vulkan_swapchain& swapchain);
  [[nodiscard]] granit_result recreate_swapchain(VkSurfaceKHR surface,
                                                 const vulkan_swapchain_desc& desc,
                                                 vulkan_swapchain& swapchain);
  [[nodiscard]] vulkan_swapchain_info
  get_swapchain_info(const vulkan_swapchain& swapchain) noexcept;
  void destroy_native_swapchain(vulkan_swapchain& swapchain) noexcept;
  [[nodiscard]] granit_result create_native_buffer(const granit_buffer_desc& desc,
                                                   vulkan_buffer_allocation& buffer) noexcept;
  void destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept;
  [[nodiscard]] granit_result flush_buffer(const vulkan_buffer_allocation& buffer,
                                           VkDeviceSize offset, VkDeviceSize size) noexcept;
  [[nodiscard]] granit_result invalidate_buffer(const vulkan_buffer_allocation& buffer,
                                                VkDeviceSize offset, VkDeviceSize size) noexcept;
  [[nodiscard]] granit_result upload_buffer(const vulkan_buffer_allocation& buffer,
                                            VkDeviceSize offset, const void* data,
                                            VkDeviceSize size) noexcept;
  [[nodiscard]] granit_result
  upload_batch(std::span<const backend_upload_operation> uploads) noexcept;
  [[nodiscard]] granit_result create_native_texture(const granit_texture_desc& desc,
                                                    vulkan_image_allocation& texture) noexcept;
  [[nodiscard]] bool texture_supports_linear_blit(granit_texture_format format) const noexcept;
  [[nodiscard]] granit_result upload_texture(const vulkan_image_allocation& texture,
                                             const void* data, VkDeviceSize size,
                                             const VkBufferImageCopy& copy) noexcept;
  void destroy_native_texture(vulkan_image_allocation& texture) noexcept;
  [[nodiscard]] granit_result create_native_texture_view(const vulkan_image_allocation& texture,
                                                         const granit_texture_desc& texture_desc,
                                                         const granit_texture_view_desc& view_desc,
                                                         VkImageView& view) noexcept;
  void destroy_native_texture_view(VkImageView view) noexcept;
  [[nodiscard]] granit_result create_native_sampler(const granit_sampler_desc& desc,
                                                    VkSampler& sampler) noexcept;
  void destroy_native_sampler(VkSampler sampler) noexcept;
  [[nodiscard]] granit_result create_native_shader(std::span<const std::uint32_t> code,
                                                   VkShaderModule& shader) noexcept;
  void destroy_native_shader(VkShaderModule shader) noexcept;
  [[nodiscard]] granit_result
  create_native_bind_group_layout(std::span<const granit_bind_group_layout_entry> entries,
                                  VkDescriptorSetLayout& layout) noexcept;
  void destroy_native_bind_group_layout(VkDescriptorSetLayout layout) noexcept;
  [[nodiscard]] granit_result
  create_native_bind_group(VkDescriptorSetLayout layout,
                           std::span<const vulkan_bind_group_write> writes, VkDescriptorPool& pool,
                           VkDescriptorSet& set) noexcept;
  void destroy_native_bind_group(VkDescriptorPool pool) noexcept;
  [[nodiscard]] granit_result
  create_native_pipeline_layout(std::span<const VkDescriptorSetLayout> bind_group_layouts,
                                VkPipelineLayout& layout) noexcept;
  void destroy_native_pipeline_layout(VkPipelineLayout layout) noexcept;
  [[nodiscard]] granit_result create_native_graphics_pipeline(
      VkPipelineLayout layout, VkShaderModule vertex_shader, const char* vertex_entry,
      VkShaderModule fragment_shader, const char* fragment_entry,
      std::span<const granit_vertex_buffer_layout> vertex_buffers, granit_primitive_state primitive,
      granit_depth_state depth, const granit_depth_bias_state* depth_bias,
      std::span<const granit_color_blend_state> color_blends,
      std::span<const granit_texture_format> color_formats,
      granit_texture_format depth_stencil_format, granit_sample_count sample_count,
      VkPipeline& pipeline) noexcept;
  void destroy_native_graphics_pipeline(VkPipeline pipeline) noexcept;
  [[nodiscard]] granit_result create_native_compute_pipeline(VkPipelineLayout layout,
                                                             VkShaderModule compute_shader,
                                                             const char* compute_entry,
                                                             VkPipeline& pipeline) noexcept;
  void destroy_native_compute_pipeline(VkPipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  create_native_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result begin_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result end_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result reset_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result copy_buffer(vulkan_command_recorder& recorder, VkBuffer source,
                                          VkBuffer destination,
                                          std::span<const VkBufferCopy> regions);
  [[nodiscard]] granit_result copy_texture_to_buffer(vulkan_command_recorder& recorder,
                                                     VkImage source, VkBuffer destination,
                                                     const VkBufferImageCopy& region);
  [[nodiscard]] granit_result copy_buffer_to_texture(vulkan_command_recorder& recorder,
                                                     VkBuffer source, VkImage destination,
                                                     const VkBufferImageCopy& region);
  [[nodiscard]] granit_result copy_texture(vulkan_command_recorder& recorder, VkImage source,
                                           VkImage destination, const VkImageCopy& region);
  [[nodiscard]] granit_result generate_mipmaps(vulkan_command_recorder& recorder, VkImage image,
                                               VkExtent3D base_extent, std::uint32_t base_mip_level,
                                               std::uint32_t level_count,
                                               std::uint32_t base_array_layer,
                                               std::uint32_t array_layer_count);
  [[nodiscard]] granit_result fill_buffer(vulkan_command_recorder& recorder, VkBuffer buffer,
                                          VkDeviceSize offset, VkDeviceSize size,
                                          std::uint32_t value);
  [[nodiscard]] granit_result bind_graphics_pipeline(vulkan_command_recorder& recorder,
                                                     VkPipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  bind_graphics_groups(vulkan_command_recorder& recorder, VkPipelineLayout layout,
                       std::uint32_t first_group, std::span<const VkDescriptorSet> bind_groups,
                       std::span<const std::pair<VkBuffer, VkAccessFlags2>> buffer_accesses,
                       std::span<const vulkan_image_access> image_accesses);
  [[nodiscard]] granit_result bind_compute_pipeline(vulkan_command_recorder& recorder,
                                                    VkPipeline pipeline) noexcept;
  [[nodiscard]] granit_result
  bind_compute_groups(vulkan_command_recorder& recorder, VkPipelineLayout layout,
                      std::uint32_t first_group, std::span<const VkDescriptorSet> bind_groups,
                      std::span<const std::pair<VkBuffer, VkAccessFlags2>> buffer_accesses,
                      std::span<const vulkan_image_access> image_accesses);
  [[nodiscard]] granit_result dispatch(vulkan_command_recorder& recorder,
                                       std::uint32_t group_count_x, std::uint32_t group_count_y,
                                       std::uint32_t group_count_z) noexcept;
  [[nodiscard]] granit_result set_viewports(vulkan_command_recorder& recorder, std::uint32_t first,
                                            std::span<const VkViewport> viewports) noexcept;
  [[nodiscard]] granit_result set_scissors(vulkan_command_recorder& recorder, std::uint32_t first,
                                           std::span<const VkRect2D> scissors) noexcept;
  [[nodiscard]] granit_result bind_vertex_buffers(vulkan_command_recorder& recorder,
                                                  std::uint32_t first,
                                                  std::span<const VkBuffer> buffers,
                                                  std::span<const VkDeviceSize> offsets);
  [[nodiscard]] granit_result bind_index_buffer(vulkan_command_recorder& recorder, VkBuffer buffer,
                                                VkDeviceSize offset, VkIndexType type);
  [[nodiscard]] granit_result draw(vulkan_command_recorder& recorder, std::uint32_t vertex_count,
                                   std::uint32_t instance_count, std::uint32_t first_vertex,
                                   std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result draw_indexed(vulkan_command_recorder& recorder,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance) noexcept;
  [[nodiscard]] granit_result
  begin_rendering(vulkan_command_recorder& recorder, VkRect2D area,
                  std::span<const VkRenderingAttachmentInfo> color_attachments,
                  const VkRenderingAttachmentInfo* depth_attachment,
                  const VkRenderingAttachmentInfo* stencil_attachment, std::uint32_t layer_count,
                  std::span<const vulkan_image_access> image_accesses);
  [[nodiscard]] granit_result end_rendering(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result submit_command_recorder(vulkan_command_recorder& recorder,
                                                      submission_serial& submitted_serial);
  [[nodiscard]] granit_result
  submit_command_recorders(std::span<vulkan_command_recorder* const> recorders,
                           submission_serial& submitted_serial);
  [[nodiscard]] granit_result acquire_swapchain_frame(vulkan_swapchain& swapchain,
                                                      std::uint32_t& image_index,
                                                      std::size_t& slot_index,
                                                      bool& needs_recreate);
  [[nodiscard]] granit_result submit_swapchain_frame(vulkan_command_recorder& recorder,
                                                     vulkan_swapchain& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     submission_serial& submitted_serial);
  [[nodiscard]] granit_result present_swapchain_frame(vulkan_swapchain& swapchain,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index, bool& needs_recreate);
  [[nodiscard]] granit_result cancel_swapchain_frame(vulkan_swapchain& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index, bool& needs_recreate);
  [[nodiscard]] granit_result wait_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result wait_for_all_submissions() noexcept;
  [[nodiscard]] granit_result wait_for_present_idle() noexcept;
  void retire_resource(submission_serial retire_after, retirement_order order,
                       std::shared_ptr<void> resource);
  std::size_t collect_retired() noexcept;
  std::size_t drain_retired() noexcept;
  void destroy_native_command_recorder(vulkan_command_recorder& recorder) noexcept;

  void set_domain(std::uint32_t domain) noexcept { domain_ = domain; }
  [[nodiscard]] std::uint32_t domain() const noexcept { return domain_; }
  [[nodiscard]] std::size_t frame_slot_count() const noexcept { return frame_slots_.size(); }
  [[nodiscard]] bool validation_enabled() const noexcept { return validation_enabled_; }
  [[nodiscard]] bool device_lost() const noexcept {
    return device_status_.gate() == GRANIT_ERROR_DEVICE_LOST;
  }
  [[nodiscard]] const diagnostic_sink& diagnostics() const noexcept { return diagnostics_; }
  [[nodiscard]] const backend_capabilities& capabilities() const noexcept { return capabilities_; }
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
