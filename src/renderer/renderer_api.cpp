// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.h>

#include "renderer/renderer_factory.h"
#include "renderer/renderer_registry.h"
#include "renderer/renderer_validation.h"

#include <cstring>
#include <new>
#include <string_view>

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

  try {
    return granit::detail::create_default_renderer(*desc, *renderer);
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

extern "C" granit_result
granit_renderer_get_shader_capabilities(granit_renderer renderer,
                                        granit_renderer_shader_capabilities* capabilities) {
  if (capabilities == nullptr ||
      capabilities->struct_size < GRANIT_RENDERER_SHADER_CAPABILITIES_SIZE ||
      capabilities->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().get_shader_capabilities(renderer,
                                                                                 *capabilities);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_renderer_select_shader_variant(granit_renderer renderer,
                                      const granit_shader_variant_requirement* variants,
                                      uint32_t variant_count, uint32_t* selected_index) {
  if (selected_index == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *selected_index = UINT32_MAX;
  if (variants == nullptr || variant_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < variant_count; ++index) {
    const auto& variant = variants[index];
    if (variant.struct_size < sizeof(granit_shader_variant_requirement) ||
        (variant.backend != GRANIT_RENDERER_BACKEND_VULKAN &&
         variant.backend != GRANIT_RENDERER_BACKEND_WEBGPU) ||
        variant.profile != GRANIT_SHADER_PROFILE_PORTABLE ||
        (variant.required_features & ~GRANIT_SHADER_FEATURE_ALL_BITS) != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().select_shader_variant(
        renderer, std::span{variants, variant_count}, *selected_index);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_get_info(granit_renderer renderer,
                                                  granit_renderer_info* info) {
  if (info == nullptr || info->struct_size < GRANIT_RENDERER_INFO_VERSION_1_SIZE ||
      info->reserved[0] != 0 || info->reserved[1] != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().get_info(renderer, *info);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_get_resource_stats(granit_renderer renderer,
                                                            granit_renderer_resource_stats* stats) {
  if (stats == nullptr || stats->struct_size < GRANIT_RENDERER_RESOURCE_STATS_VERSION_1_SIZE ||
      stats->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().get_resource_stats(renderer, *stats);
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
