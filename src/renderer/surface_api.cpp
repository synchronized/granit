// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/surface.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_surface_create_win32(granit_renderer renderer,
                                                     const granit_win32_surface_desc* desc,
                                                     granit_surface* surface) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || surface == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *surface = GRANIT_NULL_HANDLE;
  if (desc->struct_size < GRANIT_WIN32_SURFACE_DESC_VERSION_1_SIZE || desc->instance == nullptr ||
      desc->window == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().create_win32_surface(
        renderer, desc->instance, desc->window, *surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_surface_destroy(granit_renderer renderer, granit_surface surface) {
  if (renderer == GRANIT_NULL_HANDLE || surface == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_surface(renderer, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
