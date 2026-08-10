// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.h>

#include "renderer/renderer_registry.h"

#include <cstring>
#include <new>
#include <string_view>

namespace {

constexpr std::uint32_t supported_flags = GRANIT_RENDERER_ENABLE_VALIDATION_BIT;
constexpr std::uint32_t supported_surface_types = GRANIT_SURFACE_TYPE_WIN32_BIT;
constexpr std::uint32_t maximum_application_name_length = 4096;
constexpr std::string_view default_application_name = "Granit Application";

granit_result validate_desc(const granit_renderer_desc& desc) noexcept {
  if (desc.struct_size < GRANIT_RENDERER_DESC_VERSION_1_SIZE ||
      desc.api_version != GRANIT_RENDERER_API_VERSION_CURRENT ||
      (desc.flags & ~supported_flags) != 0 ||
      desc.application_name_length > maximum_application_name_length) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_2_SIZE &&
      (desc.surface_types & ~supported_surface_types) != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_3_SIZE &&
      (desc.reserved != 0 || desc.frames_in_flight == 0 ||
       desc.frames_in_flight > GRANIT_MAX_FRAMES_IN_FLIGHT)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.application_name == nullptr) {
    return desc.application_name_length == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.application_name_length == 0 ||
      std::memchr(desc.application_name, '\0', desc.application_name_length) != nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

} // namespace

extern "C" granit_result granit_renderer_create(const granit_renderer_desc* desc,
                                                granit_renderer* renderer) {
  if (desc == nullptr || renderer == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *renderer = GRANIT_NULL_HANDLE;
  const auto validation_result = validate_desc(*desc);
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
  try {
    return granit::detail::renderer_registry::instance().create(
        application_name, validation_enabled, surface_types, frames_in_flight, *renderer);
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

extern "C" granit_result granit_renderer_pipeline_cache_import(granit_renderer renderer,
                                                               const void* data, uint64_t size) {
  if (renderer == GRANIT_NULL_HANDLE || (data == nullptr && size != 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().import_pipeline_cache(renderer, data,
                                                                               size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_pipeline_cache_export(granit_renderer renderer, void* data,
                                                               uint64_t* size) {
  if (renderer == GRANIT_NULL_HANDLE || size == nullptr || (data != nullptr && *size == 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().export_pipeline_cache(renderer, data,
                                                                               *size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
