// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/buffer.h>

#include "core/resource_validation.h"
#include "renderer/renderer_registry.h"

extern "C" granit_result granit_buffer_create(granit_renderer renderer,
                                              const granit_buffer_desc* desc,
                                              granit_buffer* buffer) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || buffer == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *buffer = GRANIT_NULL_HANDLE;
  const auto validation_result = granit::detail::validate_buffer_desc(*desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  try {
    return granit::detail::renderer_registry::instance().create_buffer(renderer, *desc, *buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_map(granit_renderer renderer, granit_buffer buffer,
                                           uint64_t offset, uint64_t size, void** data) {
  if (data == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *data = nullptr;
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().map_buffer(renderer, buffer, offset, size,
                                                                    *data);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_unmap(granit_renderer renderer, granit_buffer buffer) {
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().unmap_buffer(renderer, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_destroy(granit_renderer renderer, granit_buffer buffer) {
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_buffer(renderer, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
