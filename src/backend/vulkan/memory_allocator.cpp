// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/memory_allocator.h"

#include <utility>

#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/result.h"

namespace granit::detail {
namespace {

VmaAllocationCreateInfo make_allocation_info(vulkan_memory_location location) noexcept {
  VmaAllocationCreateInfo info{};
  switch (location) {
  case vulkan_memory_location::automatic:
    info.usage = VMA_MEMORY_USAGE_AUTO;
    break;
  case vulkan_memory_location::device:
    info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    break;
  case vulkan_memory_location::upload:
    info.usage = VMA_MEMORY_USAGE_AUTO;
    info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;
    break;
  case vulkan_memory_location::readback:
    info.usage = VMA_MEMORY_USAGE_AUTO;
    info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;
    break;
  }
  return info;
}

} // namespace

vulkan_memory_allocator::~vulkan_memory_allocator() { reset(); }

vulkan_memory_allocator::vulkan_memory_allocator(vulkan_memory_allocator&& other) noexcept
    : allocator_(std::exchange(other.allocator_, VK_NULL_HANDLE)) {}

vulkan_memory_allocator&
vulkan_memory_allocator::operator=(vulkan_memory_allocator&& other) noexcept {
  if (this != &other) {
    reset();
    allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
  }
  return *this;
}

granit_result vulkan_memory_allocator::initialize(const vulkan_instance& instance,
                                                  const vulkan_device& device) noexcept {
  if (valid() || !instance.valid() || !device.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const auto& instance_functions = instance.functions();
  const auto& device_functions = device.functions();
  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = volk::vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = volk::vkGetDeviceProcAddr;
  functions.vkGetPhysicalDeviceProperties = instance_functions.vkGetPhysicalDeviceProperties;
  functions.vkGetPhysicalDeviceMemoryProperties =
      instance_functions.vkGetPhysicalDeviceMemoryProperties;
  functions.vkAllocateMemory = device_functions.vkAllocateMemory;
  functions.vkFreeMemory = device_functions.vkFreeMemory;
  functions.vkMapMemory = device_functions.vkMapMemory;
  functions.vkUnmapMemory = device_functions.vkUnmapMemory;
  functions.vkFlushMappedMemoryRanges = device_functions.vkFlushMappedMemoryRanges;
  functions.vkInvalidateMappedMemoryRanges = device_functions.vkInvalidateMappedMemoryRanges;
  functions.vkBindBufferMemory = device_functions.vkBindBufferMemory;
  functions.vkBindImageMemory = device_functions.vkBindImageMemory;
  functions.vkGetBufferMemoryRequirements = device_functions.vkGetBufferMemoryRequirements;
  functions.vkGetImageMemoryRequirements = device_functions.vkGetImageMemoryRequirements;
  functions.vkCreateBuffer = device_functions.vkCreateBuffer;
  functions.vkDestroyBuffer = device_functions.vkDestroyBuffer;
  functions.vkCreateImage = device_functions.vkCreateImage;
  functions.vkDestroyImage = device_functions.vkDestroyImage;
  functions.vkCmdCopyBuffer = device_functions.vkCmdCopyBuffer;
  functions.vkGetBufferMemoryRequirements2KHR = device_functions.vkGetBufferMemoryRequirements2;
  functions.vkGetImageMemoryRequirements2KHR = device_functions.vkGetImageMemoryRequirements2;
  functions.vkBindBufferMemory2KHR = device_functions.vkBindBufferMemory2;
  functions.vkBindImageMemory2KHR = device_functions.vkBindImageMemory2;
  functions.vkGetPhysicalDeviceMemoryProperties2KHR =
      instance_functions.vkGetPhysicalDeviceMemoryProperties2;
  functions.vkGetDeviceBufferMemoryRequirements =
      device_functions.vkGetDeviceBufferMemoryRequirements;
  functions.vkGetDeviceImageMemoryRequirements =
      device_functions.vkGetDeviceImageMemoryRequirements;

  VmaAllocatorCreateInfo create_info{};
  create_info.flags = VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
  create_info.physicalDevice = device.physical_device();
  create_info.device = device.native_handle();
  create_info.pVulkanFunctions = &functions;
  create_info.instance = instance.native_handle();
  create_info.vulkanApiVersion = VK_API_VERSION_1_3;
  return map_vulkan_result(vmaCreateAllocator(&create_info, &allocator_));
}

void vulkan_memory_allocator::reset() noexcept {
  if (allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(allocator_);
    allocator_ = VK_NULL_HANDLE;
  }
}

granit_result vulkan_memory_allocator::create_buffer(const VkBufferCreateInfo& create_info,
                                                     vulkan_memory_location location,
                                                     vulkan_buffer_allocation& buffer) noexcept {
  if (!valid() || buffer.buffer != VK_NULL_HANDLE || buffer.allocation != VK_NULL_HANDLE ||
      create_info.size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto allocation_info = make_allocation_info(location);
  VmaAllocationInfo result_info{};
  const auto result = vmaCreateBuffer(allocator_, &create_info, &allocation_info, &buffer.buffer,
                                      &buffer.allocation, &result_info);
  if (result != VK_SUCCESS) {
    buffer = {};
    return map_vulkan_result(result);
  }
  buffer.mapped_data = result_info.pMappedData;
  return GRANIT_SUCCESS;
}

void vulkan_memory_allocator::destroy_buffer(vulkan_buffer_allocation& buffer) noexcept {
  if (valid() && buffer.buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
  }
  buffer = {};
}

granit_result vulkan_memory_allocator::create_image(const VkImageCreateInfo& create_info,
                                                    vulkan_memory_location location,
                                                    vulkan_image_allocation& image) noexcept {
  if (!valid() || image.image != VK_NULL_HANDLE || image.allocation != VK_NULL_HANDLE ||
      create_info.extent.width == 0 || create_info.extent.height == 0 ||
      create_info.extent.depth == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto allocation_info = make_allocation_info(location);
  const auto result = vmaCreateImage(allocator_, &create_info, &allocation_info, &image.image,
                                     &image.allocation, nullptr);
  if (result != VK_SUCCESS) {
    image = {};
    return map_vulkan_result(result);
  }
  return GRANIT_SUCCESS;
}

void vulkan_memory_allocator::destroy_image(vulkan_image_allocation& image) noexcept {
  if (valid() && image.image != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator_, image.image, image.allocation);
  }
  image = {};
}

granit_result vulkan_memory_allocator::flush(const vulkan_buffer_allocation& buffer,
                                             VkDeviceSize offset, VkDeviceSize size) noexcept {
  if (!valid() || buffer.allocation == VK_NULL_HANDLE || buffer.mapped_data == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return map_vulkan_result(vmaFlushAllocation(allocator_, buffer.allocation, offset, size));
}

granit_result vulkan_memory_allocator::invalidate(const vulkan_buffer_allocation& buffer,
                                                  VkDeviceSize offset,
                                                  VkDeviceSize size) noexcept {
  if (!valid() || buffer.allocation == VK_NULL_HANDLE || buffer.mapped_data == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return map_vulkan_result(vmaInvalidateAllocation(allocator_, buffer.allocation, offset, size));
}

} // namespace granit::detail
