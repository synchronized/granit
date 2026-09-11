// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/surface.h>

#include "renderer/renderer_registry.h"
#include "renderer/surface_validation.h"

extern "C" granit_result granit_surface_create(granit_renderer renderer,
                                               const granit_surface_desc* desc,
                                               granit_surface* surface) {
  if (surface == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *surface = GRANIT_NULL_HANDLE;
  if (desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto validation_result = granit::detail::validate_surface_desc(desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  auto normalized = *desc;
  if (normalized.surface_type == GRANIT_SURFACE_TYPE_CANVAS_BIT &&
      normalized.source.canvas.selector == nullptr) {
    normalized.source.canvas.selector = granit::detail::default_canvas_selector.data();
    normalized.source.canvas.selector_length =
        static_cast<uint32_t>(granit::detail::default_canvas_selector.size());
  }
  try {
    return granit::detail::renderer_registry::instance().create_surface(renderer, normalized,
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
