// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_STATE_H_
#define GRANIT_RENDERER_RENDERER_STATE_H_

#include <cstdint>
#include <mutex>
#include <string_view>

#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>

#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/memory_allocator.h"
#include "backend/vulkan/swapchain.h"

namespace granit::detail {

class renderer_state {
public:
  renderer_state() = default;

  renderer_state(const renderer_state&) = delete;
  renderer_state& operator=(const renderer_state&) = delete;
  renderer_state(renderer_state&&) = delete;
  renderer_state& operator=(renderer_state&&) = delete;

  [[nodiscard]] granit_result initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types);

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

  void set_domain(std::uint32_t domain) noexcept { domain_ = domain; }
  [[nodiscard]] std::uint32_t domain() const noexcept { return domain_; }
  [[nodiscard]] const vulkan_instance& instance() const noexcept { return instance_; }
  [[nodiscard]] const vulkan_device& device() const noexcept { return device_; }

private:
  std::uint32_t domain_{};
  std::uint32_t surface_types_{};
  std::mutex resource_mutex_;
  vulkan_instance instance_;
  vulkan_device device_;
  vulkan_memory_allocator memory_allocator_;
};

} // namespace granit::detail

#endif
