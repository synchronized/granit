// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_SWAPCHAIN_H_
#define GRANIT_BACKEND_VULKAN_SWAPCHAIN_H_

#include <cstdint>
#include <vector>

#include <granit/core/result.h>

#include <volk.h>

namespace granit::detail {

class vulkan_device;
class vulkan_instance;

struct vulkan_swapchain_desc {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t minimum_image_count{};
  std::uint32_t present_mode{};
};

struct vulkan_swapchain_info {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t image_count{};
  std::uint32_t present_mode{};
  VkFormat format{VK_FORMAT_UNDEFINED};
};

struct vulkan_acquire_result {
  granit_result result{GRANIT_ERROR_UNKNOWN};
  std::uint32_t image_index{};
  bool suboptimal{};
};

struct vulkan_present_result {
  granit_result result{GRANIT_ERROR_UNKNOWN};
  bool suboptimal{};
};

class vulkan_swapchain {
public:
  vulkan_swapchain() = default;
  ~vulkan_swapchain() = default;

  vulkan_swapchain(const vulkan_swapchain&) = delete;
  vulkan_swapchain& operator=(const vulkan_swapchain&) = delete;
  vulkan_swapchain(vulkan_swapchain&&) = delete;
  vulkan_swapchain& operator=(vulkan_swapchain&&) = delete;

  [[nodiscard]] granit_result initialize(const vulkan_instance& instance,
                                         const vulkan_device& device, VkSurfaceKHR surface,
                                         const vulkan_swapchain_desc& desc);
  [[nodiscard]] granit_result recreate(const vulkan_instance& instance, const vulkan_device& device,
                                       VkSurfaceKHR surface, const vulkan_swapchain_desc& desc);
  [[nodiscard]] vulkan_acquire_result acquire(const vulkan_device& device,
                                              VkSemaphore signal_semaphore) noexcept;
  [[nodiscard]] vulkan_present_result present(const vulkan_device& device, VkQueue queue,
                                              std::uint32_t image_index,
                                              VkSemaphore wait_semaphore) noexcept;
  void reset(const vulkan_device& device) noexcept;

  [[nodiscard]] bool valid() const noexcept { return handle_ != VK_NULL_HANDLE; }
  [[nodiscard]] vulkan_swapchain_info info() const noexcept;
  [[nodiscard]] const std::vector<VkImage>& images() const noexcept { return images_; }
  [[nodiscard]] VkSemaphore render_finished(std::uint32_t image_index) const noexcept {
    return image_index < render_finished_.size() ? render_finished_[image_index] : VK_NULL_HANDLE;
  }
  [[nodiscard]] VkFormat format() const noexcept { return format_; }
  [[nodiscard]] VkSwapchainKHR native_handle() const noexcept { return handle_; }

private:
  VkSwapchainKHR handle_{VK_NULL_HANDLE};
  VkFormat format_{VK_FORMAT_UNDEFINED};
  VkExtent2D extent_{};
  VkPresentModeKHR present_mode_{VK_PRESENT_MODE_FIFO_KHR};
  std::vector<VkImage> images_;
  std::vector<VkSemaphore> render_finished_;
};

} // namespace granit::detail

#endif
