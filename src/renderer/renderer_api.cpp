// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.h>

#include "renderer/renderer_registry.h"
#include "renderer/renderer_validation.h"

#include <cstring>
#include <new>
#include <string_view>

namespace {

constexpr std::string_view default_application_name = "Granit Application";

} // namespace

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

  const auto application_name =
      desc->application_name == nullptr
          ? default_application_name
          : std::string_view{desc->application_name, desc->application_name_length};
  const auto validation_enabled = (desc->flags & GRANIT_RENDERER_ENABLE_VALIDATION_BIT) != 0;
  const auto surface_types =
      desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_2_SIZE ? desc->surface_types : UINT32_C(0);
  const auto frames_in_flight = desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_3_SIZE
                                    ? desc->frames_in_flight
                                    : GRANIT_DEFAULT_FRAMES_IN_FLIGHT;
  const auto diagnostic_callback = desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE
                                       ? desc->diagnostic_callback
                                       : nullptr;
  auto* diagnostic_user_data = desc->struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE
                                   ? desc->diagnostic_user_data
                                   : nullptr;
  try {
    return granit::detail::renderer_registry::instance().create(
        application_name, validation_enabled, surface_types, frames_in_flight, diagnostic_callback,
        diagnostic_user_data, *renderer);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_destroy(granit_renderer renderer) {
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy(renderer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_get_limits(granit_renderer renderer,
                                                    granit_renderer_limits* limits) {
  if (limits == nullptr || limits->struct_size < GRANIT_RENDERER_LIMITS_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().get_limits(renderer, *limits);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
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
  try {
    return granit::detail::renderer_registry::instance().get_status(renderer, *status);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_process_events(granit_renderer renderer) {
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().process_events(renderer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_set_object_name(granit_renderer renderer,
                                                         granit_handle object, const char* name,
                                                         uint32_t name_length) {
  if (renderer == GRANIT_NULL_HANDLE || object == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (name == nullptr || name_length == 0 ||
      name_length > granit::detail::maximum_renderer_object_name_length ||
      std::memchr(name, '\0', name_length) != nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().set_object_name(
        renderer, object, std::string_view{name, name_length});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_pipeline_cache_import(granit_renderer renderer,
                                                               const void* data, uint64_t size) {
  if (data == nullptr && size != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().import_pipeline_cache(renderer, data,
                                                                               size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_pipeline_cache_export(granit_renderer renderer, void* data,
                                                               uint64_t* size) {
  if (size == nullptr || (data != nullptr && *size == 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().export_pipeline_cache(renderer, data,
                                                                               *size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
