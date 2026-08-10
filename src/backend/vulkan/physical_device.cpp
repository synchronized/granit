// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/physical_device.h"

#include "backend/vulkan/result.h"

#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <vector>

#include <granit/renderer/renderer.h>

namespace granit::detail {
namespace {

std::uint8_t kind_rank(physical_device_kind kind) noexcept {
  switch (kind) {
  case physical_device_kind::discrete_gpu:
    return 5;
  case physical_device_kind::integrated_gpu:
    return 4;
  case physical_device_kind::virtual_gpu:
    return 3;
  case physical_device_kind::cpu:
    return 2;
  case physical_device_kind::other:
    return 1;
  }
  return 0;
}

physical_device_kind map_device_kind(VkPhysicalDeviceType type) noexcept {
  switch (type) {
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    return physical_device_kind::discrete_gpu;
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    return physical_device_kind::integrated_gpu;
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    return physical_device_kind::virtual_gpu;
  case VK_PHYSICAL_DEVICE_TYPE_CPU:
    return physical_device_kind::cpu;
  default:
    return physical_device_kind::other;
  }
}

std::uint64_t query_device_local_memory(const volk::VolkInstanceTable& functions,
                                        VkPhysicalDevice device) noexcept {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  functions.vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

  std::uint64_t total = 0;
  for (std::uint32_t index = 0; index < memory_properties.memoryHeapCount; ++index) {
    if ((memory_properties.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
      const auto size = memory_properties.memoryHeaps[index].size;
      if (size > std::numeric_limits<std::uint64_t>::max() - total) {
        return std::numeric_limits<std::uint64_t>::max();
      }
      total += size;
    }
  }
  return total;
}

std::optional<std::uint32_t> find_graphics_queue(const volk::VolkInstanceTable& functions,
                                                 VkPhysicalDevice device) {
  std::uint32_t queue_count = 0;
  functions.vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, nullptr);
  std::vector<VkQueueFamilyProperties> queues(queue_count);
  if (queue_count != 0) {
    functions.vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, queues.data());
  }
  for (std::uint32_t index = 0; index < queue_count; ++index) {
    if (queues[index].queueCount != 0 && (queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      return index;
    }
  }
  return std::nullopt;
}

bool device_extension_available(const volk::VolkInstanceTable& functions, VkPhysicalDevice device,
                                const char* required_name) {
  std::uint32_t extension_count = 0;
  if (functions.vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr) !=
      VK_SUCCESS) {
    return false;
  }
  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count != 0 &&
      functions.vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                                     extensions.data()) != VK_SUCCESS) {
    return false;
  }
  for (const auto& extension : extensions) {
    if (std::strcmp(extension.extensionName, required_name) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace

bool is_suitable(const physical_device_candidate& candidate) noexcept {
  return candidate.api_version >= VK_API_VERSION_1_3 && candidate.has_graphics_queue &&
         candidate.dynamic_rendering && candidate.synchronization2 && candidate.maintenance4 &&
         candidate.supports_requested_surfaces && candidate.supports_swapchain;
}

bool is_better_candidate(const physical_device_candidate& candidate,
                         const physical_device_candidate& current) noexcept {
  if (!is_suitable(candidate)) {
    return false;
  }
  if (!is_suitable(current)) {
    return true;
  }
  const auto candidate_rank = kind_rank(candidate.kind);
  const auto current_rank = kind_rank(current.kind);
  if (candidate_rank != current_rank) {
    return candidate_rank > current_rank;
  }
  if (candidate.device_local_memory != current.device_local_memory) {
    return candidate.device_local_memory > current.device_local_memory;
  }
  return candidate.enumeration_index < current.enumeration_index;
}

granit_result select_physical_device(const volk::VolkInstanceTable& functions, VkInstance instance,
                                     std::uint32_t surface_types,
                                     selected_physical_device& selected) {
  if (instance == VK_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  try {
    std::uint32_t device_count = 0;
    auto result = functions.vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS) {
      return map_vulkan_result(result);
    }
    if (device_count == 0) {
      return GRANIT_ERROR_NO_SUITABLE_DEVICE;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    result = functions.vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
      return map_vulkan_result(result);
    }
    devices.resize(device_count);

    bool found = false;
    physical_device_candidate best{};
    for (std::uint32_t index = 0; index < device_count; ++index) {
      VkPhysicalDeviceProperties properties{};
      functions.vkGetPhysicalDeviceProperties(devices[index], &properties);

      VkPhysicalDeviceVulkan13Features features{};
      features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
      VkPhysicalDeviceFeatures2 features2{};
      features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      features2.pNext = &features;
      functions.vkGetPhysicalDeviceFeatures2(devices[index], &features2);

      const auto graphics_queue = find_graphics_queue(functions, devices[index]);
      bool supports_requested_surfaces = true;
      if ((surface_types & GRANIT_SURFACE_TYPE_WIN32_BIT) != 0) {
#if defined(_WIN32)
        supports_requested_surfaces =
            graphics_queue.has_value() &&
            functions.vkGetPhysicalDeviceWin32PresentationSupportKHR != nullptr &&
            functions.vkGetPhysicalDeviceWin32PresentationSupportKHR(devices[index],
                                                                     *graphics_queue) == VK_TRUE;
#else
        supports_requested_surfaces = false;
#endif
      }
      const auto local_memory = query_device_local_memory(functions, devices[index]);
      const auto supports_swapchain =
          surface_types == 0 ||
          device_extension_available(functions, devices[index], VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      const physical_device_candidate candidate{
          .kind = map_device_kind(properties.deviceType),
          .api_version = properties.apiVersion,
          .device_local_memory = local_memory,
          .enumeration_index = index,
          .has_graphics_queue = graphics_queue.has_value(),
          .dynamic_rendering = features.dynamicRendering == VK_TRUE,
          .synchronization2 = features.synchronization2 == VK_TRUE,
          .maintenance4 = features.maintenance4 == VK_TRUE,
          .supports_requested_surfaces = supports_requested_surfaces,
          .supports_swapchain = supports_swapchain,
      };

      if (!found || is_better_candidate(candidate, best)) {
        if (!is_suitable(candidate)) {
          continue;
        }
        found = true;
        best = candidate;
        selected.handle = devices[index];
        selected.properties = properties;
        selected.graphics_queue_family = *graphics_queue;
        selected.device_local_memory = local_memory;
        selected.sampler_anisotropy = features2.features.samplerAnisotropy == VK_TRUE;
        selected.fill_mode_non_solid = features2.features.fillModeNonSolid == VK_TRUE;
      }
    }
    return found ? GRANIT_SUCCESS : GRANIT_ERROR_NO_SUITABLE_DEVICE;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

} // namespace granit::detail
