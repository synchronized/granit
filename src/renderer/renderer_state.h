// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_STATE_H_
#define GRANIT_RENDERER_RENDERER_STATE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>

#include "backend/vulkan/command_recorder.h"
#include "backend/vulkan/device.h"
#include "backend/vulkan/frame_context.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/memory_allocator.h"
#include "backend/vulkan/swapchain.h"
#include "core/retirement_queue.h"

namespace granit::detail {

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
                                         std::uint32_t frames_in_flight);

  [[nodiscard]] granit_result create_win32_surface(void* native_instance, void* native_window,
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
  [[nodiscard]] granit_result create_native_texture(const granit_texture_desc& desc,
                                                    vulkan_image_allocation& texture) noexcept;
  void destroy_native_texture(vulkan_image_allocation& texture) noexcept;
  [[nodiscard]] granit_result create_native_texture_view(const vulkan_image_allocation& texture,
                                                         const granit_texture_desc& texture_desc,
                                                         const granit_texture_view_desc& view_desc,
                                                         VkImageView& view) noexcept;
  void destroy_native_texture_view(VkImageView view) noexcept;
  [[nodiscard]] granit_result create_native_sampler(const granit_sampler_desc& desc,
                                                    VkSampler& sampler) noexcept;
  void destroy_native_sampler(VkSampler sampler) noexcept;
  [[nodiscard]] granit_result
  create_native_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result begin_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result end_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result reset_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result copy_buffer(vulkan_command_recorder& recorder, VkBuffer source,
                                          VkBuffer destination,
                                          std::span<const VkBufferCopy> regions);
  [[nodiscard]] granit_result fill_buffer(vulkan_command_recorder& recorder, VkBuffer buffer,
                                          VkDeviceSize offset, VkDeviceSize size,
                                          std::uint32_t value);
  [[nodiscard]] granit_result
  begin_rendering(vulkan_command_recorder& recorder, VkRect2D area,
                  std::span<const VkRenderingAttachmentInfo> color_attachments,
                  const VkRenderingAttachmentInfo* depth_attachment,
                  const VkRenderingAttachmentInfo* stencil_attachment, std::uint32_t layer_count,
                  std::span<const vulkan_image_access> image_accesses);
  [[nodiscard]] granit_result end_rendering(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result submit_command_recorder(vulkan_command_recorder& recorder);
  [[nodiscard]] granit_result acquire_swapchain_frame(vulkan_swapchain& swapchain,
                                                      std::uint32_t& image_index,
                                                      std::size_t& slot_index,
                                                      bool& needs_recreate);
  [[nodiscard]] granit_result submit_swapchain_frame(vulkan_command_recorder& recorder,
                                                     vulkan_swapchain& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index);
  [[nodiscard]] granit_result present_swapchain_frame(vulkan_swapchain& swapchain,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index, bool& needs_recreate);
  [[nodiscard]] granit_result wait_command_recorder(vulkan_command_recorder& recorder) noexcept;
  [[nodiscard]] granit_result wait_for_all_submissions() noexcept;
  void destroy_native_command_recorder(vulkan_command_recorder& recorder) noexcept;

  void set_domain(std::uint32_t domain) noexcept { domain_ = domain; }
  [[nodiscard]] std::uint32_t domain() const noexcept { return domain_; }
  [[nodiscard]] bool validation_enabled() const noexcept { return validation_enabled_; }
  [[nodiscard]] const vulkan_instance& instance() const noexcept { return instance_; }
  [[nodiscard]] const vulkan_device& device() const noexcept { return device_; }

private:
  struct frame_slot {
    std::unique_ptr<vulkan_frame_context> context;
    std::unique_ptr<vulkan_command_recorder> preamble;
    std::unique_ptr<vulkan_command_recorder> postamble;
    vulkan_command_recorder* recorder{};
    submission_serial serial{};
    bool acquired{};
    bool awaiting_present{};
  };

  [[nodiscard]] granit_result complete_frame_slot(frame_slot& slot) noexcept;

  std::uint32_t domain_{};
  std::uint32_t surface_types_{};
  bool validation_enabled_{};
  std::mutex resource_mutex_;
  std::mutex queue_mutex_;
  vulkan_instance instance_;
  vulkan_device device_;
  vulkan_memory_allocator memory_allocator_;
  std::vector<frame_slot> frame_slots_;
  std::size_t next_frame_slot_{};
  submission_serials submission_serials_;
  std::vector<vulkan_image_access> image_states_;
};

} // namespace granit::detail

#endif
