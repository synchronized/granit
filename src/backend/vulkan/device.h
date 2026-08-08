// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_DEVICE_H_
#define GRANIT_BACKEND_VULKAN_DEVICE_H_

#include <cstdint>

#include <granit/result.h>

#include <volk.h>

namespace granit::detail {

class vulkan_instance;

/** 拥有逻辑设备、graphics queue 和独立 device 函数表。 */
class vulkan_device {
public:
  vulkan_device() = default;
  ~vulkan_device();

  vulkan_device(const vulkan_device&) = delete;
  vulkan_device& operator=(const vulkan_device&) = delete;
  vulkan_device(vulkan_device&& other) noexcept;
  vulkan_device& operator=(vulkan_device&& other) noexcept;

  [[nodiscard]] granit_result initialize(const vulkan_instance& instance,
                                         std::uint32_t surface_types = 0);
  void reset() noexcept;

  [[nodiscard]] bool valid() const noexcept { return device_ != VK_NULL_HANDLE; }
  [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
  [[nodiscard]] VkDevice native_handle() const noexcept { return device_; }
  [[nodiscard]] VkQueue graphics_queue() const noexcept { return graphics_queue_; }
  [[nodiscard]] std::uint32_t graphics_queue_family() const noexcept {
    return graphics_queue_family_;
  }
  [[nodiscard]] const VkPhysicalDeviceProperties& properties() const noexcept {
    return properties_;
  }
  [[nodiscard]] const volk::VolkDeviceTable& functions() const noexcept { return functions_; }
  [[nodiscard]] bool sampler_anisotropy_supported() const noexcept {
    return sampler_anisotropy_supported_;
  }

private:
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  std::uint32_t graphics_queue_family_{};
  VkPhysicalDeviceProperties properties_{};
  volk::VolkDeviceTable functions_{};
  bool sampler_anisotropy_supported_{};
};

} // namespace granit::detail

#endif
