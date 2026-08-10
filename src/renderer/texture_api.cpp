// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/texture.hpp>

#include "core/resource_validation.h"
#include "renderer/renderer_registry.h"

extern "C" granit_result granit_texture_create(granit_renderer renderer,
                                               const granit_texture_desc* desc,
                                               granit_texture* texture) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || texture == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *texture = GRANIT_NULL_HANDLE;
  const auto result = granit::detail::validate_texture_desc(*desc);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  return granit::detail::renderer_registry::instance().create_texture(renderer, *desc, *texture);
}

extern "C" granit_result granit_texture_view_create(granit_renderer renderer,
                                                    granit_texture texture,
                                                    const granit_texture_view_desc* desc,
                                                    granit_texture_view* view) {
  if (renderer == GRANIT_NULL_HANDLE || texture == GRANIT_NULL_HANDLE || desc == nullptr ||
      view == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *view = GRANIT_NULL_HANDLE;
  const auto result = granit::detail::validate_texture_view_desc(*desc);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  return granit::detail::renderer_registry::instance().create_texture_view(renderer, texture, *desc,
                                                                           *view);
}

extern "C" granit_result granit_texture_create_with_default_view(granit_renderer renderer,
                                                                 const granit_texture_desc* desc,
                                                                 granit_texture* texture,
                                                                 granit_texture_view* view) {
  if (texture == nullptr || view == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *texture = GRANIT_NULL_HANDLE;
  *view = GRANIT_NULL_HANDLE;
  granit_texture created = GRANIT_NULL_HANDLE;
  auto result = granit_texture_create(renderer, desc, &created);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit_texture_view_desc view_desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  result = granit_texture_view_create(renderer, created, &view_desc, view);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_texture_destroy(renderer, created));
    return result;
  }
  *texture = created;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_texture_view_destroy(granit_renderer renderer,
                                                     granit_texture_view view) {
  if (renderer == GRANIT_NULL_HANDLE || view == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_texture_view(renderer, view);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_texture_destroy(granit_renderer renderer, granit_texture texture) {
  if (renderer == GRANIT_NULL_HANDLE || texture == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_texture(renderer, texture);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
