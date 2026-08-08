// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_PHYSICAL_DEVICE_H_
#define GRANIT_BACKEND_VULKAN_PHYSICAL_DEVICE_H_

#include <cstdint>

#include <granit/result.h>

#include <volk.h>

namespace granit::detail {

enum class physical_device_kind : std::uint8_t {
  other,
  cpu,
  virtual_gpu,
  integrated_gpu,
  discrete_gpu,
};

/** 与 Vulkan 句柄无关的设备选择输入，便于确定性测试选择策略。 */
struct physical_device_candidate {
  physical_device_kind kind{physical_device_kind::other};
  std::uint32_t api_version{};
  std::uint64_t device_local_memory{};
  std::uint32_t enumeration_index{};
  bool has_graphics_queue{};
  bool dynamic_rendering{};
  bool synchronization2{};
  bool maintenance4{};
  bool supports_requested_surfaces{true};
  bool supports_swapchain{true};
};

struct selected_physical_device {
  VkPhysicalDevice handle{VK_NULL_HANDLE};
  VkPhysicalDeviceProperties properties{};
  std::uint32_t graphics_queue_family{};
  std::uint64_t device_local_memory{};
  bool sampler_anisotropy{};
};

[[nodiscard]] bool is_suitable(const physical_device_candidate& candidate) noexcept;
[[nodiscard]] bool is_better_candidate(const physical_device_candidate& candidate,
                                       const physical_device_candidate& current) noexcept;

/** 从 instance 可见设备中选择满足 Granit 基础要求的最佳设备。 */
[[nodiscard]] granit_result select_physical_device(const volk::VolkInstanceTable& functions,
                                                   VkInstance instance, std::uint32_t surface_types,
                                                   selected_physical_device& selected);

} // namespace granit::detail

#endif
