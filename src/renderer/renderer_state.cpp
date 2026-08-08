// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

#include "backend/vulkan/surface.h"

namespace granit::detail {

granit_result renderer_state::initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types) {
  const auto instance_result = instance_.initialize({.application_name = application_name,
                                                     .enable_validation = enable_validation,
                                                     .surface_types = surface_types});
  if (instance_result != GRANIT_SUCCESS) {
    return instance_result;
  }

  const auto device_result = device_.initialize(instance_, surface_types);
  if (device_result != GRANIT_SUCCESS) {
    instance_.reset();
    return device_result;
  }

  const auto allocator_result = memory_allocator_.initialize(instance_, device_);
  if (allocator_result != GRANIT_SUCCESS) {
    device_.reset();
    instance_.reset();
    return allocator_result;
  }
  surface_types_ = surface_types;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::create_win32_surface(void* native_instance, void* native_window,
                                                   VkSurfaceKHR& surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return detail::create_win32_surface(instance_, device_, native_instance, native_window, surface);
}

void renderer_state::destroy_native_surface(VkSurfaceKHR surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  detail::destroy_surface(instance_, surface);
}

granit_result renderer_state::create_swapchain(VkSurfaceKHR surface,
                                               const vulkan_swapchain_desc& desc,
                                               vulkan_swapchain& swapchain) {
  std::lock_guard lock{resource_mutex_};
  return swapchain.initialize(instance_, device_, surface, desc);
}

granit_result renderer_state::recreate_swapchain(VkSurfaceKHR surface,
                                                 const vulkan_swapchain_desc& desc,
                                                 vulkan_swapchain& swapchain) {
  std::lock_guard lock{resource_mutex_};
  return swapchain.recreate(instance_, device_, surface, desc);
}

vulkan_swapchain_info
renderer_state::get_swapchain_info(const vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  return swapchain.info();
}

void renderer_state::destroy_native_swapchain(vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  swapchain.reset(device_);
}

} // namespace granit::detail
