// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/upload_context.h"

#include <bit>
#include <cstdint>
#include <limits>

#include "backend/vulkan/device.h"
#include "backend/vulkan/result.h"

namespace granit::detail {

granit_result vulkan_upload_context::initialize(const vulkan_device& device) noexcept {
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags =
      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = device.graphics_queue_family();
  auto result = map_vulkan_result(device.functions().vkCreateCommandPool(
      device.native_handle(), &pool_info, nullptr, &command_pool_));
  if (result != GRANIT_SUCCESS)
    return result;

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool_;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;
  result = map_vulkan_result(device.functions().vkAllocateCommandBuffers(
      device.native_handle(), &allocate_info, &command_buffer_));
  if (result == GRANIT_SUCCESS)
    result = fence_.initialize(device, true);
  if (result != GRANIT_SUCCESS) {
    if (command_pool_ != VK_NULL_HANDLE)
      device.functions().vkDestroyCommandPool(device.native_handle(), command_pool_, nullptr);
    command_pool_ = VK_NULL_HANDLE;
    command_buffer_ = VK_NULL_HANDLE;
  }
  return result;
}

granit_result vulkan_upload_context::ensure_capacity(vulkan_memory_allocator& allocator,
                                                     VkDeviceSize required) noexcept {
  if (required <= capacity_)
    return GRANIT_SUCCESS;
  if (required > (std::numeric_limits<VkDeviceSize>::max() / 2) + 1)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto capacity = std::bit_ceil(required);
  VkBufferCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.size = capacity;
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vulkan_buffer_allocation replacement;
  const auto result = allocator.create_buffer(info, vulkan_memory_location::upload, replacement);
  if (result != GRANIT_SUCCESS)
    return result;
  allocator.destroy_buffer(staging_);
  staging_ = replacement;
  capacity_ = capacity;
  return GRANIT_SUCCESS;
}

granit_result vulkan_upload_context::begin(const vulkan_device& device) noexcept {
  auto result = map_vulkan_result(
      device.functions().vkResetCommandPool(device.native_handle(), command_pool_, 0));
  if (result != GRANIT_SUCCESS)
    return result;
  VkCommandBufferBeginInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  return map_vulkan_result(device.functions().vkBeginCommandBuffer(command_buffer_, &info));
}

granit_result vulkan_upload_context::end(const vulkan_device& device) noexcept {
  return map_vulkan_result(device.functions().vkEndCommandBuffer(command_buffer_));
}

granit_result vulkan_upload_context::reset_fence(const vulkan_device& device) noexcept {
  return fence_.reset(device);
}

granit_result vulkan_upload_context::wait(const vulkan_device& device) noexcept {
  return fence_.wait(device, UINT64_MAX);
}

granit_result vulkan_upload_context::restore_signaled_fence(const vulkan_device& device) noexcept {
  fence_.destroy(device);
  return fence_.initialize(device, true);
}

void vulkan_upload_context::destroy(const vulkan_device& device,
                                    vulkan_memory_allocator& allocator) noexcept {
  allocator.destroy_buffer(staging_);
  capacity_ = 0;
  fence_.destroy(device);
  if (command_pool_ != VK_NULL_HANDLE && device.valid())
    device.functions().vkDestroyCommandPool(device.native_handle(), command_pool_, nullptr);
  command_pool_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
}

} // namespace granit::detail
