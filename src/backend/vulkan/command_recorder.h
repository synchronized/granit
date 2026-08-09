// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_COMMAND_RECORDER_H_
#define GRANIT_BACKEND_VULKAN_COMMAND_RECORDER_H_

#include <granit/result.h>

#include <span>
#include <unordered_map>
#include <vector>

#include <volk.h>

namespace granit::detail {

class vulkan_device;

enum class command_recorder_state { initial, recording, executable, pending, invalid };

struct vulkan_image_access {
  VkImage image{VK_NULL_HANDLE};
  VkImageSubresourceRange range{};
  VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkPipelineStageFlags2 stages{};
  VkAccessFlags2 access{};
  bool preserve_content{};
};

/** 拥有独立 Command Pool 和主 Command Buffer 的一次性录制器。 */
class vulkan_command_recorder {
public:
  [[nodiscard]] granit_result initialize(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result begin(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result end(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result reset(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result copy_buffer(const vulkan_device& device, VkBuffer source,
                                          VkBuffer destination,
                                          std::span<const VkBufferCopy> regions);
  [[nodiscard]] granit_result fill_buffer(const vulkan_device& device, VkBuffer buffer,
                                          VkDeviceSize offset, VkDeviceSize size,
                                          std::uint32_t value);
  [[nodiscard]] granit_result
  begin_rendering(const vulkan_device& device, VkRect2D area,
                  std::span<const VkRenderingAttachmentInfo> color_attachments,
                  const VkRenderingAttachmentInfo* depth_attachment,
                  const VkRenderingAttachmentInfo* stencil_attachment, std::uint32_t layer_count,
                  std::span<const vulkan_image_access> image_accesses);
  [[nodiscard]] granit_result end_rendering(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result
  record_image_barriers(const vulkan_device& device,
                        std::span<const VkImageMemoryBarrier2> barriers) noexcept;
  [[nodiscard]] granit_result mark_pending() noexcept;
  void mark_complete() noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] command_recorder_state state() const noexcept { return state_; }
  [[nodiscard]] VkCommandBuffer native_handle() const noexcept { return command_buffer_; }
  [[nodiscard]] std::span<const vulkan_image_access> initial_image_accesses() const noexcept {
    return initial_image_accesses_;
  }
  [[nodiscard]] std::span<const vulkan_image_access> final_image_accesses() const noexcept {
    return final_image_accesses_;
  }

private:
  struct buffer_access_state {
    VkPipelineStageFlags2 stages{};
    VkAccessFlags2 access{};
  };

  [[nodiscard]] granit_result
  prepare_buffer_access(const vulkan_device& device,
                        std::span<const std::pair<VkBuffer, VkAccessFlags2>> accesses);
  void prepare_image_access(const vulkan_device& device, const vulkan_image_access& access);

  VkCommandPool pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  command_recorder_state state_{command_recorder_state::invalid};
  bool inside_rendering_{};
  std::unordered_map<VkBuffer, buffer_access_state> buffer_accesses_;
  std::vector<vulkan_image_access> initial_image_accesses_;
  std::vector<vulkan_image_access> final_image_accesses_;
};

} // namespace granit::detail

#endif
