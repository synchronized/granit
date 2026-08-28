// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/swapchain.h>

#include "renderer/renderer_registry.h"
#include "renderer/swapchain_validation.h"

extern "C" granit_result granit_swapchain_create(granit_renderer renderer, granit_surface surface,
                                                 const granit_swapchain_desc* desc,
                                                 granit_swapchain* swapchain) {
  if (swapchain == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *swapchain = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || surface == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto validation_result = granit::detail::validate_swapchain_desc(desc, false);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  return granit::detail::renderer_registry::instance().create_swapchain(
      renderer, surface, granit::detail::to_backend_swapchain_desc(*desc), *swapchain);
}

extern "C" granit_result granit_swapchain_recreate(granit_renderer renderer,
                                                   granit_swapchain swapchain,
                                                   const granit_swapchain_desc* desc) {
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto validation_result = granit::detail::validate_swapchain_desc(desc, true);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  return granit::detail::renderer_registry::instance().recreate_swapchain(
      renderer, swapchain, granit::detail::to_backend_swapchain_desc(*desc));
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
  granit::detail::backend_swapchain_info backend_info{};
  const auto result = granit::detail::renderer_registry::instance().get_swapchain_info(
      renderer, swapchain, backend_info);
  if (result == GRANIT_SUCCESS) {
    info->width = backend_info.width;
    info->height = backend_info.height;
    info->image_count = backend_info.image_count;
    info->present_mode = backend_info.present_mode;
    if (info->struct_size >= GRANIT_SWAPCHAIN_INFO_VERSION_2_SIZE) {
      info->format = backend_info.format;
    }
  }
  return result;
}

extern "C" granit_result granit_swapchain_get_backbuffer(granit_renderer renderer,
                                                         granit_swapchain swapchain, uint32_t index,
                                                         granit_texture* texture,
                                                         granit_texture_view* view) {
  if (texture == nullptr || view == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *texture = GRANIT_NULL_HANDLE;
  *view = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::renderer_registry::instance().get_swapchain_backbuffer(
      renderer, swapchain, index, *texture, *view);
}

extern "C" granit_result granit_swapchain_acquire(granit_renderer renderer,
                                                  granit_swapchain swapchain, granit_frame* frame,
                                                  uint32_t* image_index, uint32_t* needs_recreate) {
  if (frame == nullptr || image_index == nullptr || needs_recreate == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *frame = GRANIT_NULL_HANDLE;
  *image_index = 0;
  *needs_recreate = 0;
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  bool recreate{};
  const auto result = granit::detail::renderer_registry::instance().acquire_swapchain(
      renderer, swapchain, *frame, *image_index, recreate);
  *needs_recreate = recreate ? 1U : 0U;
  return result;
}

extern "C" granit_result granit_frame_get_info(granit_renderer renderer, granit_swapchain swapchain,
                                               granit_frame frame, granit_frame_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (info == nullptr || info->struct_size < GRANIT_FRAME_INFO_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto result = granit::detail::renderer_registry::instance().get_frame_info(
      renderer, swapchain, frame, info->frame_slot, info->frame_slot_count);
  if (result == GRANIT_SUCCESS) {
    for (auto& value : info->reserved) {
      value = 0;
    }
  }
  return result;
}

namespace {

granit_result finish_frame(granit_renderer renderer, granit_swapchain swapchain, granit_frame frame,
                           uint32_t* needs_recreate, bool present) {
  if (needs_recreate == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *needs_recreate = 0;
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  bool recreate{};
  const auto result = granit::detail::renderer_registry::instance().finish_frame(
      renderer, swapchain, frame, present, recreate);
  *needs_recreate = recreate ? 1U : 0U;
  return result;
}

} // namespace

extern "C" granit_result granit_swapchain_present(granit_renderer renderer,
                                                  granit_swapchain swapchain, granit_frame frame,
                                                  uint32_t* needs_recreate) {
  return finish_frame(renderer, swapchain, frame, needs_recreate, true);
}

extern "C" granit_result granit_frame_cancel(granit_renderer renderer, granit_swapchain swapchain,
                                             granit_frame frame, uint32_t* needs_recreate) {
  return finish_frame(renderer, swapchain, frame, needs_recreate, false);
}

extern "C" granit_result granit_swapchain_destroy(granit_renderer renderer,
                                                  granit_swapchain swapchain) {
  if (renderer == GRANIT_NULL_HANDLE || swapchain == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::renderer_registry::instance().destroy_swapchain(renderer, swapchain);
}
