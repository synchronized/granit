// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_SURFACE_H_
#define GRANIT_BACKEND_VULKAN_SURFACE_H_

#include <cstdint>

#include <granit/core/result.h>

#include <volk.h>

namespace granit::detail {

class vulkan_device;
class vulkan_instance;

[[nodiscard]] granit_result create_win32_surface(const vulkan_instance& instance,
                                                 const vulkan_device& device, void* native_instance,
                                                 void* native_window,
                                                 VkSurfaceKHR& surface) noexcept;

[[nodiscard]] granit_result create_xcb_surface(const vulkan_instance& instance,
                                               const vulkan_device& device, void* connection,
                                               std::uint32_t window,
                                               VkSurfaceKHR& surface) noexcept;

void destroy_surface(const vulkan_instance& instance, VkSurfaceKHR surface) noexcept;

} // namespace granit::detail

#endif
