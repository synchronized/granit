// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/device.h"

#include "backend/vulkan/instance.h"
#include "backend/vulkan/physical_device.h"
#include "backend/vulkan/result.h"

#include <utility>

namespace granit::detail {

vulkan_device::~vulkan_device() { reset(); }

vulkan_device::vulkan_device(vulkan_device&& other) noexcept
    : physical_device_(std::exchange(other.physical_device_, VK_NULL_HANDLE)),
      device_(std::exchange(other.device_, VK_NULL_HANDLE)),
      graphics_queue_(std::exchange(other.graphics_queue_, VK_NULL_HANDLE)),
      graphics_queue_family_(std::exchange(other.graphics_queue_family_, 0)),
      properties_(other.properties_), functions_(other.functions_),
      sampler_anisotropy_supported_(other.sampler_anisotropy_supported_) {
  other.properties_ = {};
  other.functions_ = {};
  other.sampler_anisotropy_supported_ = false;
}

vulkan_device& vulkan_device::operator=(vulkan_device&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  reset();
  physical_device_ = std::exchange(other.physical_device_, VK_NULL_HANDLE);
  device_ = std::exchange(other.device_, VK_NULL_HANDLE);
  graphics_queue_ = std::exchange(other.graphics_queue_, VK_NULL_HANDLE);
  graphics_queue_family_ = std::exchange(other.graphics_queue_family_, 0);
  properties_ = other.properties_;
  functions_ = other.functions_;
  sampler_anisotropy_supported_ = other.sampler_anisotropy_supported_;
  other.properties_ = {};
  other.functions_ = {};
  other.sampler_anisotropy_supported_ = false;
  return *this;
}

granit_result vulkan_device::initialize(const vulkan_instance& instance,
                                        std::uint32_t surface_types) {
  if (valid() || !instance.valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  selected_physical_device selected{};
  const auto select_result = select_physical_device(instance.functions(), instance.native_handle(),
                                                    surface_types, selected);
  if (select_result != GRANIT_SUCCESS) {
    return select_result;
  }

  constexpr float queue_priority = 1.0F;
  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = selected.graphics_queue_family;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkPhysicalDeviceVulkan13Features features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features.synchronization2 = VK_TRUE;
  features.dynamicRendering = VK_TRUE;
  features.maintenance4 = VK_TRUE;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &features;
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_create_info;
  VkPhysicalDeviceFeatures core_features{};
  core_features.samplerAnisotropy = selected.sampler_anisotropy ? VK_TRUE : VK_FALSE;
  create_info.pEnabledFeatures = &core_features;
  const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  if (surface_types != 0) {
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = extensions;
  }

  const auto create_result =
      instance.functions().vkCreateDevice(selected.handle, &create_info, nullptr, &device_);
  if (create_result != VK_SUCCESS) {
    device_ = VK_NULL_HANDLE;
    return map_vulkan_result(create_result);
  }

  physical_device_ = selected.handle;
  properties_ = selected.properties;
  sampler_anisotropy_supported_ = selected.sampler_anisotropy;
  graphics_queue_family_ = selected.graphics_queue_family;
  volk::volkLoadDeviceTable(&functions_, device_);
  if (functions_.vkGetDeviceQueue == nullptr || functions_.vkDestroyDevice == nullptr ||
      functions_.vkCreateCommandPool == nullptr || functions_.vkDestroyCommandPool == nullptr ||
      functions_.vkAllocateCommandBuffers == nullptr ||
      functions_.vkBeginCommandBuffer == nullptr || functions_.vkEndCommandBuffer == nullptr ||
      functions_.vkResetCommandPool == nullptr ||
      (surface_types != 0 &&
       (functions_.vkCreateSwapchainKHR == nullptr || functions_.vkDestroySwapchainKHR == nullptr ||
        functions_.vkGetSwapchainImagesKHR == nullptr))) {
    reset();
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  functions_.vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
  if (graphics_queue_ == VK_NULL_HANDLE) {
    reset();
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  return GRANIT_SUCCESS;
}

void vulkan_device::reset() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    if (functions_.vkDeviceWaitIdle != nullptr) {
      static_cast<void>(functions_.vkDeviceWaitIdle(device_));
    }
    if (functions_.vkDestroyDevice != nullptr) {
      functions_.vkDestroyDevice(device_, nullptr);
    }
  }
  physical_device_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  graphics_queue_ = VK_NULL_HANDLE;
  graphics_queue_family_ = 0;
  properties_ = {};
  functions_ = {};
  sampler_anisotropy_supported_ = false;
}

} // namespace granit::detail
