// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/surface.h>

#include "renderer/surface_validation.h"
#include "renderer_registry.h"

extern "C" granit_result granit_surface_create_win32(granit_renderer,
                                                     const granit_win32_surface_desc*,
                                                     granit_surface* surface) {
  if (surface != nullptr) {
    *surface = GRANIT_NULL_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_surface_create_xcb(granit_renderer, const granit_xcb_surface_desc*,
                                                   granit_surface* surface) {
  if (surface != nullptr) {
    *surface = GRANIT_NULL_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_surface_create_wayland(granit_renderer,
                                                       const granit_wayland_surface_desc*,
                                                       granit_surface* surface) {
  if (surface != nullptr) {
    *surface = GRANIT_NULL_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_surface_create_canvas(granit_renderer renderer,
                                                      const granit_canvas_surface_desc* desc,
                                                      granit_surface* surface) {
  if (surface == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *surface = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto validation_result = granit::detail::validate_canvas_surface_desc(desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  const auto selector = desc->selector == nullptr
                            ? granit::detail::default_canvas_selector
                            : std::string_view{desc->selector, desc->selector_length};
  return granit::detail::web_renderer_registry::instance().create_canvas_surface(
      renderer, selector.data(), static_cast<std::uint32_t>(selector.size()), *surface);
}

extern "C" granit_result granit_surface_destroy(granit_renderer renderer, granit_surface surface) {
  if (renderer == GRANIT_NULL_HANDLE || surface == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::web_renderer_registry::instance().destroy_surface(renderer, surface);
}
