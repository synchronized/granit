// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/swapchain.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/result.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

#include <granit/renderer/swapchain.h>

namespace granit::detail {
namespace {

VkPresentModeKHR requested_present_mode(std::uint32_t mode) noexcept {
  switch (mode) {
  case GRANIT_PRESENT_MODE_MAILBOX:
    return VK_PRESENT_MODE_MAILBOX_KHR;
  case GRANIT_PRESENT_MODE_IMMEDIATE:
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  default:
    return VK_PRESENT_MODE_FIFO_KHR;
  }
}

std::uint32_t public_present_mode(VkPresentModeKHR mode) noexcept {
  switch (mode) {
  case VK_PRESENT_MODE_MAILBOX_KHR:
    return GRANIT_PRESENT_MODE_MAILBOX;
  case VK_PRESENT_MODE_IMMEDIATE_KHR:
    return GRANIT_PRESENT_MODE_IMMEDIATE;
  default:
    return GRANIT_PRESENT_MODE_FIFO;
  }
}

VkCompositeAlphaFlagBitsKHR choose_composite_alpha(VkCompositeAlphaFlagsKHR supported) noexcept {
  constexpr VkCompositeAlphaFlagBitsKHR choices[] = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };
  for (const auto choice : choices) {
    if ((supported & choice) != 0) {
      return choice;
    }
  }
  return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

} // namespace

granit_result vulkan_swapchain::initialize(const vulkan_instance& instance,
                                           const vulkan_device& device, VkSurfaceKHR surface,
                                           const vulkan_swapchain_desc& desc) {
  if (valid()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return recreate(instance, device, surface, desc);
}

granit_result vulkan_swapchain::recreate(const vulkan_instance& instance,
                                         const vulkan_device& device, VkSurfaceKHR surface,
                                         const vulkan_swapchain_desc& desc) {
  if (!instance.valid() || !device.valid() || surface == VK_NULL_HANDLE || desc.width == 0 ||
      desc.height == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  VkSwapchainKHR pending_handle = VK_NULL_HANDLE;
  try {
    VkSurfaceCapabilitiesKHR capabilities{};
    auto result = instance.functions().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device.physical_device(), surface, &capabilities);
    if (result != VK_SUCCESS) {
      return map_vulkan_result(result);
    }
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
      return GRANIT_ERROR_UNSUPPORTED;
    }

    std::uint32_t format_count = 0;
    result = instance.functions().vkGetPhysicalDeviceSurfaceFormatsKHR(
        device.physical_device(), surface, &format_count, nullptr);
    if (result != VK_SUCCESS || format_count == 0) {
      return result == VK_SUCCESS ? GRANIT_ERROR_UNSUPPORTED : map_vulkan_result(result);
    }
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    result = instance.functions().vkGetPhysicalDeviceSurfaceFormatsKHR(
        device.physical_device(), surface, &format_count, formats.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
      return map_vulkan_result(result);
    }
    formats.resize(format_count);
    VkSurfaceFormatKHR selected_format{};
    for (const auto& format : formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        selected_format = format;
        break;
      }
    }
    if (selected_format.format == VK_FORMAT_UNDEFINED) {
      for (const auto& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_UNORM ||
             format.format == VK_FORMAT_R8G8B8A8_SRGB ||
             format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          selected_format = format;
          break;
        }
      }
    }
    if (selected_format.format == VK_FORMAT_UNDEFINED) {
      return GRANIT_ERROR_UNSUPPORTED;
    }

    std::uint32_t mode_count = 0;
    result = instance.functions().vkGetPhysicalDeviceSurfacePresentModesKHR(
        device.physical_device(), surface, &mode_count, nullptr);
    if (result != VK_SUCCESS || mode_count == 0) {
      return result == VK_SUCCESS ? GRANIT_ERROR_UNSUPPORTED : map_vulkan_result(result);
    }
    std::vector<VkPresentModeKHR> modes(mode_count);
    result = instance.functions().vkGetPhysicalDeviceSurfacePresentModesKHR(
        device.physical_device(), surface, &mode_count, modes.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
      return map_vulkan_result(result);
    }
    modes.resize(mode_count);
    const auto requested_mode = requested_present_mode(desc.present_mode);
    const auto selected_mode = std::find(modes.begin(), modes.end(), requested_mode) != modes.end()
                                   ? requested_mode
                                   : VK_PRESENT_MODE_FIFO_KHR;

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
      extent.width = std::clamp(desc.width, capabilities.minImageExtent.width,
                                capabilities.maxImageExtent.width);
      extent.height = std::clamp(desc.height, capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) {
      return GRANIT_ERROR_NOT_READY;
    }

    auto image_count = desc.minimum_image_count == 0
                           ? capabilities.minImageCount + 1
                           : std::max(desc.minimum_image_count, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0) {
      image_count = std::min(image_count, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = selected_format.format;
    create_info.imageColorSpace = selected_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = choose_composite_alpha(capabilities.supportedCompositeAlpha);
    create_info.presentMode = selected_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = handle_;

    result = device.functions().vkCreateSwapchainKHR(device.native_handle(), &create_info, nullptr,
                                                     &pending_handle);
    if (result != VK_SUCCESS) {
      return map_vulkan_result(result);
    }

    std::uint32_t actual_image_count = 0;
    result = device.functions().vkGetSwapchainImagesKHR(device.native_handle(), pending_handle,
                                                        &actual_image_count, nullptr);
    std::vector<VkImage> new_images(actual_image_count);
    if (result == VK_SUCCESS && actual_image_count != 0) {
      result = device.functions().vkGetSwapchainImagesKHR(device.native_handle(), pending_handle,
                                                          &actual_image_count, new_images.data());
    }
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || actual_image_count == 0) {
      device.functions().vkDestroySwapchainKHR(device.native_handle(), pending_handle, nullptr);
      pending_handle = VK_NULL_HANDLE;
      return result == VK_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : map_vulkan_result(result);
    }
    new_images.resize(actual_image_count);

    std::vector<VkSemaphore> new_render_finished(actual_image_count);
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& semaphore : new_render_finished) {
      result = device.functions().vkCreateSemaphore(device.native_handle(), &semaphore_info,
                                                    nullptr, &semaphore);
      if (result != VK_SUCCESS) {
        for (const auto created : new_render_finished) {
          if (created != VK_NULL_HANDLE)
            device.functions().vkDestroySemaphore(device.native_handle(), created, nullptr);
        }
        device.functions().vkDestroySwapchainKHR(device.native_handle(), pending_handle, nullptr);
        pending_handle = VK_NULL_HANDLE;
        return map_vulkan_result(result);
      }
    }

    if (handle_ != VK_NULL_HANDLE) {
      for (const auto semaphore : render_finished_)
        device.functions().vkDestroySemaphore(device.native_handle(), semaphore, nullptr);
      device.functions().vkDestroySwapchainKHR(device.native_handle(), handle_, nullptr);
    }
    handle_ = std::exchange(pending_handle, VK_NULL_HANDLE);
    format_ = selected_format.format;
    extent_ = extent;
    present_mode_ = selected_mode;
    images_ = std::move(new_images);
    render_finished_ = std::move(new_render_finished);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    if (pending_handle != VK_NULL_HANDLE) {
      device.functions().vkDestroySwapchainKHR(device.native_handle(), pending_handle, nullptr);
    }
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

vulkan_acquire_result vulkan_swapchain::acquire(const vulkan_device& device,
                                                VkSemaphore signal_semaphore) noexcept {
  if (!valid() || !device.valid() || signal_semaphore == VK_NULL_HANDLE) {
    return {.result = GRANIT_ERROR_INVALID_ARGUMENT};
  }
  std::uint32_t image_index{};
  const auto result = device.functions().vkAcquireNextImageKHR(
      device.native_handle(), handle_, UINT64_MAX, signal_semaphore, VK_NULL_HANDLE, &image_index);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
    if (image_index >= images_.size()) {
      return {.result = GRANIT_ERROR_INTERNAL};
    }
    return {.result = GRANIT_SUCCESS,
            .image_index = image_index,
            .suboptimal = result == VK_SUBOPTIMAL_KHR};
  }
  return {.result = map_vulkan_result(result)};
}

vulkan_present_result vulkan_swapchain::present(const vulkan_device& device, VkQueue queue,
                                                std::uint32_t image_index,
                                                VkSemaphore wait_semaphore) noexcept {
  if (!valid() || !device.valid() || queue == VK_NULL_HANDLE || wait_semaphore == VK_NULL_HANDLE ||
      image_index >= images_.size()) {
    return {.result = GRANIT_ERROR_INVALID_ARGUMENT};
  }
  VkPresentInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &wait_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &handle_;
  info.pImageIndices = &image_index;
  const auto result = device.functions().vkQueuePresentKHR(queue, &info);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
    return {.result = GRANIT_SUCCESS, .suboptimal = result == VK_SUBOPTIMAL_KHR};
  }
  return {.result = map_vulkan_result(result)};
}

void vulkan_swapchain::reset(const vulkan_device& device) noexcept {
  if (device.valid() && device.functions().vkDestroySemaphore != nullptr) {
    for (const auto semaphore : render_finished_)
      device.functions().vkDestroySemaphore(device.native_handle(), semaphore, nullptr);
  }
  render_finished_.clear();
  if (handle_ != VK_NULL_HANDLE && device.valid() &&
      device.functions().vkDestroySwapchainKHR != nullptr) {
    device.functions().vkDestroySwapchainKHR(device.native_handle(), handle_, nullptr);
  }
  handle_ = VK_NULL_HANDLE;
  format_ = VK_FORMAT_UNDEFINED;
  extent_ = {};
  present_mode_ = VK_PRESENT_MODE_FIFO_KHR;
  images_.clear();
}

vulkan_swapchain_info vulkan_swapchain::info() const noexcept {
  return {.width = extent_.width,
          .height = extent_.height,
          .image_count = static_cast<std::uint32_t>(images_.size()),
          .present_mode = public_present_mode(present_mode_),
          .format = format_};
}

} // namespace granit::detail
