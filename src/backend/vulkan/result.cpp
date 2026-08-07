// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/result.h"

namespace granit::detail {

granit_result map_vulkan_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return GRANIT_SUCCESS;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return GRANIT_ERROR_OUT_OF_MEMORY;
  case VK_ERROR_DEVICE_LOST:
    return GRANIT_ERROR_DEVICE_LOST;
  case VK_ERROR_LAYER_NOT_PRESENT:
  case VK_ERROR_EXTENSION_NOT_PRESENT:
  case VK_ERROR_FEATURE_NOT_PRESENT:
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return GRANIT_ERROR_UNSUPPORTED;
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  case VK_ERROR_INITIALIZATION_FAILED:
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  case VK_ERROR_UNKNOWN:
    return GRANIT_ERROR_UNKNOWN;
  default:
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::detail
