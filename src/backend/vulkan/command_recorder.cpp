// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/command_recorder.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/result.h"

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
  if (state_ != command_recorder_state::recording) {
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
  return map_vulkan_result(result);
}

void vulkan_command_recorder::destroy(const vulkan_device& device) noexcept {
  if (pool_ != VK_NULL_HANDLE) {
    device.functions().vkDestroyCommandPool(device.native_handle(), pool_, nullptr);
  }
  pool_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
  state_ = command_recorder_state::invalid;
}

} // namespace granit::detail
