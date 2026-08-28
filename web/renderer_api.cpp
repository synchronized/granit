// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.h>

#include "renderer/renderer_validation.h"
#include "renderer_registry.h"

#include <cstring>

extern "C" granit_result granit_renderer_create(const granit_renderer_desc* desc,
                                                granit_renderer* renderer) {
  if (desc == nullptr || renderer == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *renderer = GRANIT_NULL_HANDLE;
  const auto validation_result = granit::detail::validate_renderer_desc(*desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  if (desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_2_SIZE &&
      (desc->surface_types & ~GRANIT_SURFACE_TYPE_CANVAS_BIT) != 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const auto callback = desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE
                            ? desc->diagnostic_callback
                            : nullptr;
  auto* user_data = desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE
                        ? desc->diagnostic_user_data
                        : nullptr;
  return granit::detail::web_renderer_registry::instance().create(callback, user_data, *renderer);
}

extern "C" granit_result granit_renderer_destroy(granit_renderer renderer) {
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::web_renderer_registry::instance().destroy(renderer);
}

extern "C" granit_result granit_renderer_get_limits(granit_renderer renderer,
                                                    granit_renderer_limits* limits) {
  if (limits == nullptr || limits->struct_size < GRANIT_RENDERER_LIMITS_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::web_renderer_registry::instance().get_limits(renderer, *limits);
}

extern "C" granit_result granit_renderer_get_status(granit_renderer renderer,
                                                    granit_renderer_status* status) {
  if (status == nullptr || status->struct_size < GRANIT_RENDERER_STATUS_VERSION_1_SIZE ||
      status->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::web_renderer_registry::instance().get_status(renderer, *status);
}

extern "C" granit_result granit_renderer_process_events(granit_renderer renderer) {
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::web_renderer_registry::instance().process_events(renderer);
}

extern "C" granit_result granit_renderer_set_object_name(granit_renderer renderer,
                                                         granit_handle object, const char* name,
                                                         uint32_t name_length) {
  if (renderer == GRANIT_NULL_HANDLE || object == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (name == nullptr || name_length == 0 ||
      name_length > granit::detail::maximum_renderer_object_name_length ||
      std::memchr(name, '\0', name_length) != nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_renderer_pipeline_cache_import(granit_renderer renderer,
                                                               const void* data, uint64_t size) {
  if (data == nullptr && size != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_renderer_pipeline_cache_export(granit_renderer renderer, void* data,
                                                               uint64_t* size) {
  if (size == nullptr || (data != nullptr && *size == 0)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}
