// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/command_recorder.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/result.h"

#include <algorithm>
#include <array>
#include <vector>

namespace granit::detail {

granit_result vulkan_command_recorder::initialize(const vulkan_device& device) noexcept {
  if (pool_ != VK_NULL_HANDLE || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto& functions = device.functions();
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = device.graphics_queue_family();
  auto result = functions.vkCreateCommandPool(device.native_handle(), &pool_info, nullptr, &pool_);
  if (result != VK_SUCCESS) {
    pool_ = VK_NULL_HANDLE;
    return map_vulkan_result(result);
  }

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = pool_;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;
  result =
      functions.vkAllocateCommandBuffers(device.native_handle(), &allocate_info, &command_buffer_);
  if (result != VK_SUCCESS) {
    destroy(device);
    return map_vulkan_result(result);
  }
  state_ = command_recorder_state::initial;
  buffer_accesses_.clear();
  initial_image_accesses_.clear();
  final_image_accesses_.clear();
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::begin(const vulkan_device& device) noexcept {
  if (state_ != command_recorder_state::initial) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  const auto result = device.functions().vkBeginCommandBuffer(command_buffer_, &begin_info);
  if (result == VK_SUCCESS) {
    state_ = command_recorder_state::recording;
  }
  return map_vulkan_result(result);
}

granit_result vulkan_command_recorder::end(const vulkan_device& device) noexcept {
  if (state_ != command_recorder_state::recording || inside_rendering_) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto result = device.functions().vkEndCommandBuffer(command_buffer_);
  state_ =
      result == VK_SUCCESS ? command_recorder_state::executable : command_recorder_state::invalid;
  return map_vulkan_result(result);
}

granit_result vulkan_command_recorder::reset(const vulkan_device& device) noexcept {
  if (state_ == command_recorder_state::recording || state_ == command_recorder_state::pending ||
      pool_ == VK_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto result = device.functions().vkResetCommandPool(device.native_handle(), pool_, 0);
  state_ = result == VK_SUCCESS ? command_recorder_state::initial : command_recorder_state::invalid;
  inside_rendering_ = false;
  buffer_accesses_.clear();
  initial_image_accesses_.clear();
  final_image_accesses_.clear();
  return map_vulkan_result(result);
}

granit_result vulkan_command_recorder::prepare_buffer_access(
    const vulkan_device& device, std::span<const std::pair<VkBuffer, VkAccessFlags2>> accesses) {
  std::vector<std::pair<VkBuffer, VkAccessFlags2>> merged;
  merged.reserve(accesses.size());
  for (const auto& access : accesses) {
    const auto found = std::find_if(merged.begin(), merged.end(), [&](const auto& candidate) {
      return candidate.first == access.first;
    });
    if (found == merged.end()) {
      merged.push_back(access);
    } else {
      found->second |= access.second;
    }
  }

  std::vector<VkBufferMemoryBarrier2> barriers;
  barriers.reserve(merged.size());
  for (const auto& [buffer, destination_access] : merged) {
    const auto previous = buffer_accesses_.find(buffer);
    const buffer_access_state source =
        previous == buffer_accesses_.end()
            ? buffer_access_state{.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                  .access =
                                      VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT}
            : previous->second;
    const bool read_after_read = (source.access & VK_ACCESS_2_MEMORY_WRITE_BIT) == 0 &&
                                 (destination_access & VK_ACCESS_2_MEMORY_WRITE_BIT) == 0;
    if (!read_after_read) {
      VkBufferMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
      barrier.srcStageMask = source.stages;
      barrier.srcAccessMask = source.access;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = destination_access;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = buffer;
      barrier.offset = 0;
      barrier.size = VK_WHOLE_SIZE;
      barriers.push_back(barrier);
    }
    buffer_accesses_[buffer] = {
        .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .access = destination_access,
    };
  }
  if (!barriers.empty()) {
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.bufferMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
    dependency.pBufferMemoryBarriers = barriers.data();
    device.functions().vkCmdPipelineBarrier2(command_buffer_, &dependency);
  }
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::copy_buffer(const vulkan_device& device, VkBuffer source,
                                                   VkBuffer destination,
                                                   std::span<const VkBufferCopy> regions) {
  if (state_ != command_recorder_state::recording || inside_rendering_ || regions.empty()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::array accesses{
      std::pair{source, VkAccessFlags2{VK_ACCESS_2_TRANSFER_READ_BIT}},
      std::pair{destination, VkAccessFlags2{VK_ACCESS_2_TRANSFER_WRITE_BIT}},
  };
  const auto barrier_result = prepare_buffer_access(device, accesses);
  if (barrier_result != GRANIT_SUCCESS) {
    return barrier_result;
  }
  device.functions().vkCmdCopyBuffer(command_buffer_, source, destination,
                                     static_cast<std::uint32_t>(regions.size()), regions.data());
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::fill_buffer(const vulkan_device& device, VkBuffer buffer,
                                                   VkDeviceSize offset, VkDeviceSize size,
                                                   std::uint32_t value) {
  if (state_ != command_recorder_state::recording || inside_rendering_) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::array accesses{
      std::pair{buffer, VkAccessFlags2{VK_ACCESS_2_TRANSFER_WRITE_BIT}},
  };
  const auto barrier_result = prepare_buffer_access(device, accesses);
  if (barrier_result != GRANIT_SUCCESS) {
    return barrier_result;
  }
  device.functions().vkCmdFillBuffer(command_buffer_, buffer, offset, size, value);
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::bind_graphics_pipeline(const vulkan_device& device,
                                                              VkPipeline pipeline) noexcept {
  if (state_ != command_recorder_state::recording || pipeline == VK_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  device.functions().vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::bind_graphics_groups(
    const vulkan_device& device, VkPipelineLayout layout, std::uint32_t first_group,
    std::span<const VkDescriptorSet> bind_groups) noexcept {
  if (state_ != command_recorder_state::recording || layout == VK_NULL_HANDLE ||
      bind_groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  device.functions().vkCmdBindDescriptorSets(
      command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, first_group,
      static_cast<std::uint32_t>(bind_groups.size()), bind_groups.data(), 0, nullptr);
  return GRANIT_SUCCESS;
}

void vulkan_command_recorder::prepare_image_access(const vulkan_device& device,
                                                   const vulkan_image_access& access) {
  const auto found = std::find_if(final_image_accesses_.begin(), final_image_accesses_.end(),
                                  [&](const auto& state) { return state.image == access.image; });
  if (found == final_image_accesses_.end()) {
    initial_image_accesses_.reserve(initial_image_accesses_.size() + 1);
    final_image_accesses_.reserve(final_image_accesses_.size() + 1);
    initial_image_accesses_.push_back(access);
    final_image_accesses_.push_back(access);
    return;
  }
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask = found->stages;
  barrier.srcAccessMask = found->access;
  barrier.dstStageMask = access.stages;
  barrier.dstAccessMask = access.access;
  barrier.oldLayout = found->layout;
  barrier.newLayout = access.layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = access.image;
  barrier.subresourceRange = access.range;
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;
  device.functions().vkCmdPipelineBarrier2(command_buffer_, &dependency);
  *found = access;
}

granit_result vulkan_command_recorder::begin_rendering(
    const vulkan_device& device, VkRect2D area,
    std::span<const VkRenderingAttachmentInfo> color_attachments,
    const VkRenderingAttachmentInfo* depth_attachment,
    const VkRenderingAttachmentInfo* stencil_attachment, std::uint32_t layer_count,
    std::span<const vulkan_image_access> image_accesses) {
  if (state_ != command_recorder_state::recording || inside_rendering_) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (const auto& access : image_accesses) {
    prepare_image_access(device, access);
  }
  VkRenderingInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  info.renderArea = area;
  info.layerCount = layer_count;
  info.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size());
  info.pColorAttachments = color_attachments.data();
  info.pDepthAttachment = depth_attachment;
  info.pStencilAttachment = stencil_attachment;
  device.functions().vkCmdBeginRendering(command_buffer_, &info);
  inside_rendering_ = true;
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::record_image_barriers(
    const vulkan_device& device, std::span<const VkImageMemoryBarrier2> barriers) noexcept {
  if (state_ != command_recorder_state::recording) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (!barriers.empty()) {
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
    dependency.pImageMemoryBarriers = barriers.data();
    device.functions().vkCmdPipelineBarrier2(command_buffer_, &dependency);
  }
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::end_rendering(const vulkan_device& device) noexcept {
  if (state_ != command_recorder_state::recording || !inside_rendering_) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  device.functions().vkCmdEndRendering(command_buffer_);
  inside_rendering_ = false;
  return GRANIT_SUCCESS;
}

granit_result vulkan_command_recorder::mark_pending() noexcept {
  if (state_ != command_recorder_state::executable) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  state_ = command_recorder_state::pending;
  return GRANIT_SUCCESS;
}

void vulkan_command_recorder::mark_complete() noexcept {
  if (state_ == command_recorder_state::pending) {
    state_ = command_recorder_state::executable;
  }
}

void vulkan_command_recorder::destroy(const vulkan_device& device) noexcept {
  if (pool_ != VK_NULL_HANDLE) {
    device.functions().vkDestroyCommandPool(device.native_handle(), pool_, nullptr);
  }
  pool_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
  state_ = command_recorder_state::invalid;
  inside_rendering_ = false;
  buffer_accesses_.clear();
  initial_image_accesses_.clear();
  final_image_accesses_.clear();
}

} // namespace granit::detail
