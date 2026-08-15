// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/texture.hpp>

#include "core/resource_validation.h"
#include "core/texture_format.h"
#include "renderer/renderer_registry.h"

#include <algorithm>
#include <cstring>

extern "C" granit_result granit_texture_format_get_footprint(
    granit_texture_format format, granit_texture_format_footprint* footprint) {
  if (footprint == nullptr ||
      footprint->struct_size < GRANIT_TEXTURE_FORMAT_FOOTPRINT_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto bytes = granit::detail::texture_format_bytes_per_block(format);
  if (bytes == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  footprint->block_width = 1;
  footprint->block_height = 1;
  footprint->bytes_per_block = bytes;
  std::fill(std::begin(footprint->reserved), std::end(footprint->reserved), 0);
  return GRANIT_SUCCESS;
}

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
  view_desc.dimension = desc->dimension;
  view_desc.range.mip_level_count = desc->mip_levels;
  view_desc.range.array_layer_count = desc->array_layers;
  result = granit_texture_view_create(renderer, created, &view_desc, view);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_texture_destroy(renderer, created));
    return result;
  }
  *texture = created;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_texture_write(granit_renderer renderer, granit_texture texture,
                                              const void* data, uint64_t size,
                                              const granit_texture_data_layout* layout,
                                              const granit_texture_write_region* region) {
  if (renderer == GRANIT_NULL_HANDLE || texture == GRANIT_NULL_HANDLE || data == nullptr ||
      size == 0 || layout == nullptr || region == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return granit::detail::renderer_registry::instance().write_texture(renderer, texture, data, size,
                                                                     *layout, *region);
}

extern "C" granit_result granit_texture_read(granit_renderer renderer, granit_texture texture,
                                             const granit_texture_write_region* region, void* data,
                                             uint64_t* data_size,
                                             granit_texture_readback_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || texture == GRANIT_NULL_HANDLE || region == nullptr ||
      data_size == nullptr || info == nullptr ||
      info->struct_size < GRANIT_TEXTURE_READBACK_INFO_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto result = granit::detail::renderer_registry::instance().get_texture_readback_info(
      renderer, texture, *region, *info);
  if (result != GRANIT_SUCCESS)
    return result;
  const auto required = info->required_size;
  const auto capacity = *data_size;
  *data_size = required;
  if (data == nullptr)
    return capacity == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
  if (capacity < required)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  granit_buffer buffer = GRANIT_NULL_HANDLE;
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  buffer_desc.memory_location = GRANIT_MEMORY_LOCATION_READBACK;
  buffer_desc.size = required;
  result = granit_buffer_create(renderer, &buffer_desc, &buffer);
  if (result == GRANIT_SUCCESS) {
    const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
    result = granit_command_recorder_create(renderer, &recorder_desc, &recorder);
  }
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin(renderer, recorder);
  const granit_texture_data_layout layout{};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_copy_texture_to_buffer(renderer, recorder, texture, buffer,
                                                            &layout, region);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end(renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_submit(renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_reset(renderer, recorder);
  void* mapped = nullptr;
  if (result == GRANIT_SUCCESS)
    result = granit_buffer_map(renderer, buffer, 0, required, &mapped);
  if (result == GRANIT_SUCCESS) {
    std::memcpy(data, mapped, static_cast<size_t>(required));
    result = granit_buffer_unmap(renderer, buffer);
  }
  if (recorder != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_command_recorder_destroy(renderer, recorder));
  if (buffer != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_buffer_destroy(renderer, buffer));
  return result;
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
