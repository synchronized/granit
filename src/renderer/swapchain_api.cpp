// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/swapchain.h>

#include "renderer/renderer_registry.h"

namespace {

granit_result validate_desc(const granit_swapchain_desc* desc) noexcept {
  if (desc == nullptr || desc->struct_size < GRANIT_SWAPCHAIN_DESC_VERSION_1_SIZE ||
      desc->width == 0 || desc->height == 0 || desc->minimum_image_count > 16 ||
      desc->present_mode > GRANIT_PRESENT_MODE_IMMEDIATE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

granit::detail::vulkan_swapchain_desc to_internal(const granit_swapchain_desc& desc) noexcept {
  return {.width = desc.width,
          .height = desc.height,
          .minimum_image_count = desc.minimum_image_count,
          .present_mode = desc.present_mode};
}

} // namespace

extern "C" granit_result granit_swapchain_create(granit_renderer renderer, granit_surface surface,
                                                 const granit_swapchain_desc* desc,
                                                 granit_swapchain* swapchain) {
  if (renderer == GRANIT_NULL_HANDLE || surface == GRANIT_NULL_HANDLE || swapchain == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *swapchain = GRANIT_NULL_HANDLE;
  const auto validation_result = validate_desc(desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  try {
    return granit::detail::renderer_registry::instance().create_swapchain(
        renderer, surface, to_internal(*desc), *swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_swapchain_recreate(granit_renderer renderer,
                                                   granit_swapchain swapchain,
                                                   const granit_swapchain_desc* desc) {
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto validation_result = validate_desc(desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  try {
    return granit::detail::renderer_registry::instance().recreate_swapchain(renderer, swapchain,
                                                                            to_internal(*desc));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_swapchain_get_info(granit_renderer renderer,
                                                   granit_swapchain swapchain,
                                                   granit_swapchain_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (info == nullptr || info->struct_size < GRANIT_SWAPCHAIN_INFO_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    granit::detail::vulkan_swapchain_info native_info{};
    const auto result = granit::detail::renderer_registry::instance().get_swapchain_info(
        renderer, swapchain, native_info);
    if (result == GRANIT_SUCCESS) {
      info->width = native_info.width;
      info->height = native_info.height;
      info->image_count = native_info.image_count;
      info->present_mode = native_info.present_mode;
    }
    return result;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_swapchain_destroy(granit_renderer renderer,
                                                  granit_swapchain swapchain) {
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_swapchain(renderer, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
