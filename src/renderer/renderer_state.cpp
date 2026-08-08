// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

#include "backend/vulkan/surface.h"

namespace granit::detail {
namespace {

VkBufferUsageFlags map_buffer_usage(granit_buffer_usage usage) noexcept {
  VkBufferUsageFlags flags{};
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_INDEX_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_UNIFORM_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_STORAGE_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_INDIRECT_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  return flags;
}

vulkan_memory_location map_memory_location(granit_memory_location location) noexcept {
  switch (location) {
  case GRANIT_MEMORY_LOCATION_DEVICE:
    return vulkan_memory_location::device;
  case GRANIT_MEMORY_LOCATION_UPLOAD:
    return vulkan_memory_location::upload;
  case GRANIT_MEMORY_LOCATION_READBACK:
    return vulkan_memory_location::readback;
  default:
    return vulkan_memory_location::automatic;
  }
}

} // namespace

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

granit_result renderer_state::create_native_buffer(const granit_buffer_desc& desc,
                                                   vulkan_buffer_allocation& buffer) noexcept {
  VkBufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = desc.size;
  create_info.usage = map_buffer_usage(desc.usage);
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  return memory_allocator_.create_buffer(create_info, map_memory_location(desc.memory_location),
                                         buffer);
}

void renderer_state::destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept {
  memory_allocator_.destroy_buffer(buffer);
}

granit_result renderer_state::flush_buffer(const vulkan_buffer_allocation& buffer,
                                           VkDeviceSize offset, VkDeviceSize size) noexcept {
  return memory_allocator_.flush(buffer, offset, size);
}

granit_result renderer_state::invalidate_buffer(const vulkan_buffer_allocation& buffer,
                                                VkDeviceSize offset, VkDeviceSize size) noexcept {
  return memory_allocator_.invalidate(buffer, offset, size);
}

} // namespace granit::detail
