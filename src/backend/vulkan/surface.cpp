// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/surface.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/result.h"

namespace granit::detail {

granit_result create_win32_surface(const vulkan_instance& instance, const vulkan_device& device,
                                   void* native_instance, void* native_window,
                                   VkSurfaceKHR& surface) noexcept {
  surface = VK_NULL_HANDLE;
#if defined(_WIN32)
  if (!instance.valid() || !device.valid() || native_instance == nullptr ||
      native_window == nullptr || instance.functions().vkCreateWin32SurfaceKHR == nullptr ||
      instance.functions().vkGetPhysicalDeviceSurfaceSupportKHR == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  VkWin32SurfaceCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  create_info.hinstance = static_cast<HINSTANCE>(native_instance);
  create_info.hwnd = static_cast<HWND>(native_window);
  const auto create_result = instance.functions().vkCreateWin32SurfaceKHR(
      instance.native_handle(), &create_info, nullptr, &surface);
  if (create_result != VK_SUCCESS) {
    return map_vulkan_result(create_result);
  }

  VkBool32 presentation_supported = VK_FALSE;
  const auto support_result = instance.functions().vkGetPhysicalDeviceSurfaceSupportKHR(
      device.physical_device(), device.graphics_queue_family(), surface, &presentation_supported);
  if (support_result != VK_SUCCESS || presentation_supported != VK_TRUE) {
    destroy_surface(instance, surface);
    surface = VK_NULL_HANDLE;
    return support_result == VK_SUCCESS ? GRANIT_ERROR_NO_SUITABLE_DEVICE
                                        : map_vulkan_result(support_result);
  }
  return GRANIT_SUCCESS;
#else
  static_cast<void>(instance);
  static_cast<void>(device);
  static_cast<void>(native_instance);
  static_cast<void>(native_window);
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_xcb_surface(const vulkan_instance& instance, const vulkan_device& device,
                                 void* connection, std::uint32_t window,
                                 VkSurfaceKHR& surface) noexcept {
  surface = VK_NULL_HANDLE;
#if defined(GRANIT_HAS_XCB)
  if (!instance.valid() || !device.valid() || connection == nullptr || window == 0 ||
      instance.functions().vkCreateXcbSurfaceKHR == nullptr ||
      instance.functions().vkGetPhysicalDeviceSurfaceSupportKHR == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  VkXcbSurfaceCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  create_info.connection = static_cast<xcb_connection_t*>(connection);
  create_info.window = static_cast<xcb_window_t>(window);
  const auto create_result = instance.functions().vkCreateXcbSurfaceKHR(
      instance.native_handle(), &create_info, nullptr, &surface);
  if (create_result != VK_SUCCESS)
    return map_vulkan_result(create_result);

  VkBool32 presentation_supported = VK_FALSE;
  const auto support_result = instance.functions().vkGetPhysicalDeviceSurfaceSupportKHR(
      device.physical_device(), device.graphics_queue_family(), surface, &presentation_supported);
  if (support_result != VK_SUCCESS || presentation_supported != VK_TRUE) {
    destroy_surface(instance, surface);
    surface = VK_NULL_HANDLE;
    return support_result == VK_SUCCESS ? GRANIT_ERROR_NO_SUITABLE_DEVICE
                                        : map_vulkan_result(support_result);
  }
  return GRANIT_SUCCESS;
#else
  static_cast<void>(instance);
  static_cast<void>(device);
  static_cast<void>(connection);
  static_cast<void>(window);
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

void destroy_surface(const vulkan_instance& instance, VkSurfaceKHR surface) noexcept {
  if (surface != VK_NULL_HANDLE && instance.valid() &&
      instance.functions().vkDestroySurfaceKHR != nullptr) {
    instance.functions().vkDestroySurfaceKHR(instance.native_handle(), surface, nullptr);
  }
}

} // namespace granit::detail
