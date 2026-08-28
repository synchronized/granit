// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/surface.h>

#include "renderer/renderer_registry.h"
#include "renderer/surface_validation.h"

#include <cstring>
#include <string_view>

extern "C" granit_result granit_surface_create_win32(granit_renderer renderer,
                                                     const granit_win32_surface_desc* desc,
                                                     granit_surface* surface) {
  if (desc == nullptr || surface == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *surface = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
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

extern "C" granit_result granit_surface_create_xcb(granit_renderer renderer,
                                                   const granit_xcb_surface_desc* desc,
                                                   granit_surface* surface) {
  if (desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *surface = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->struct_size < GRANIT_XCB_SURFACE_DESC_VERSION_1_SIZE || desc->connection == nullptr ||
      desc->window == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().create_xcb_surface(
        renderer, desc->connection, desc->window, *surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_surface_create_wayland(granit_renderer renderer,
                                                       const granit_wayland_surface_desc* desc,
                                                       granit_surface* surface) {
  if (desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *surface = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->struct_size < GRANIT_WAYLAND_SURFACE_DESC_VERSION_1_SIZE || desc->display == nullptr ||
      desc->surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().create_wayland_surface(
        renderer, desc->display, desc->surface, *surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_surface_create_canvas(granit_renderer renderer,
                                                      const granit_canvas_surface_desc* desc,
                                                      granit_surface* surface) {
  if (desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *surface = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto validation_result = granit::detail::validate_canvas_surface_desc(desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  const auto selector = desc->selector == nullptr
                            ? granit::detail::default_canvas_selector
                            : std::string_view{desc->selector, desc->selector_length};
  try {
    return granit::detail::renderer_registry::instance().create_canvas_surface(renderer, selector,
                                                                               *surface);
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
