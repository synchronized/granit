// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/frame_context.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/result.h"

namespace granit::detail {

granit_result vulkan_fence::initialize(const vulkan_device& device, bool signaled) noexcept {
  if (valid() || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  VkFenceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
  const auto result = map_vulkan_result(
      device.functions().vkCreateFence(device.native_handle(), &create_info, nullptr, &handle_));
  if (result != GRANIT_SUCCESS) {
    handle_ = VK_NULL_HANDLE;
  }
  return result;
}

granit_result vulkan_fence::wait(const vulkan_device& device, std::uint64_t timeout) noexcept {
  if (!valid() || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return map_vulkan_result(
      device.functions().vkWaitForFences(device.native_handle(), 1, &handle_, VK_TRUE, timeout));
}

granit_result vulkan_fence::reset(const vulkan_device& device) noexcept {
  if (!valid() || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return map_vulkan_result(device.functions().vkResetFences(device.native_handle(), 1, &handle_));
}

void vulkan_fence::destroy(const vulkan_device& device) noexcept {
  if (handle_ != VK_NULL_HANDLE && device.valid()) {
    device.functions().vkDestroyFence(device.native_handle(), handle_, nullptr);
  }
  handle_ = VK_NULL_HANDLE;
}

granit_result vulkan_binary_semaphore::initialize(const vulkan_device& device) noexcept {
  if (valid() || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  VkSemaphoreCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  const auto result = map_vulkan_result(device.functions().vkCreateSemaphore(
      device.native_handle(), &create_info, nullptr, &handle_));
  if (result != GRANIT_SUCCESS) {
    handle_ = VK_NULL_HANDLE;
  }
  return result;
}

void vulkan_binary_semaphore::destroy(const vulkan_device& device) noexcept {
  if (handle_ != VK_NULL_HANDLE && device.valid()) {
    device.functions().vkDestroySemaphore(device.native_handle(), handle_, nullptr);
  }
  handle_ = VK_NULL_HANDLE;
}

granit_result vulkan_frame_context::initialize(const vulkan_device& device) noexcept {
  if (valid() || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto result = completion_fence_.initialize(device, true);
  if (result == GRANIT_SUCCESS) {
    result = image_available_.initialize(device);
  }
  if (result == GRANIT_SUCCESS) {
    result = render_finished_.initialize(device);
  }
  if (result != GRANIT_SUCCESS) {
    destroy(device);
  }
  return result;
}

granit_result vulkan_frame_context::wait(const vulkan_device& device,
                                         std::uint64_t timeout) noexcept {
  return completion_fence_.wait(device, timeout);
}

granit_result vulkan_frame_context::reset_fence(const vulkan_device& device) noexcept {
  return completion_fence_.reset(device);
}

void vulkan_frame_context::destroy(const vulkan_device& device) noexcept {
  render_finished_.destroy(device);
  image_available_.destroy(device);
  completion_fence_.destroy(device);
}

} // namespace granit::detail
