// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_SWAPCHAIN_VALIDATION_H_
#define GRANIT_RENDERER_SWAPCHAIN_VALIDATION_H_

#include <granit/renderer/swapchain.h>

#include "backend/contracts/presentation.h"

namespace granit::detail {

[[nodiscard]] inline granit_result validate_swapchain_desc(const granit_swapchain_desc* desc,
                                                           bool allow_zero_extent) noexcept {
  if (desc == nullptr || desc->struct_size < GRANIT_SWAPCHAIN_DESC_VERSION_1_SIZE ||
      (!allow_zero_extent && (desc->width == 0 || desc->height == 0)) ||
      desc->present_mode > GRANIT_PRESENT_MODE_IMMEDIATE || desc->minimum_image_count > 16) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc->width == 0 || desc->height == 0) {
    return GRANIT_ERROR_NOT_READY;
  }
  return GRANIT_SUCCESS;
}

[[nodiscard]] inline backend_swapchain_desc
to_backend_swapchain_desc(const granit_swapchain_desc& desc) noexcept {
  return {.width = desc.width,
          .height = desc.height,
          .minimum_image_count = desc.minimum_image_count,
          .present_mode = desc.present_mode};
}

} // namespace granit::detail

#endif
