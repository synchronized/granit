// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "core/texture_format.h"
#include "renderer/renderer_registry_helpers.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result renderer_registry::copy_buffer(granit_renderer renderer,
                                             granit_command_recorder recorder, granit_buffer source,
                                             granit_buffer destination,
                                             std::span<const granit_buffer_copy_region> regions) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (!recorder_record->transfers)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<buffer_record> source_record;
  std::shared_ptr<buffer_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->owner;
    if (handles_.find(source, resource_type::buffer, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = buffers_.find(source);
    const auto found_destination = buffers_.find(destination);
    if (found_source == buffers_.end() || found_destination == buffers_.end() ||
        found_source->second->owner != state || found_destination->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }
  if ((source_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (destination_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (const auto& region : regions) {
    if (region.size == 0 || region.source_offset >= source_record->desc.size ||
        region.size > source_record->desc.size - region.source_offset ||
        region.destination_offset >= destination_record->desc.size ||
        region.size > destination_record->desc.size - region.destination_offset) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  if (source_record == destination_record) {
    for (const auto& source_region : regions) {
      for (const auto& destination_region : regions) {
        if (ranges_overlap(source_region.source_offset, source_region.size,
                           destination_region.destination_offset, destination_region.size)) {
          return GRANIT_ERROR_INVALID_ARGUMENT;
        }
      }
    }
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (!recorder_record->commands->command_recorder_is_recording(*recorder_record->native)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->transfers->copy_buffer(*recorder_record->native, *source_record->native,
                                                 *destination_record->native, regions);
}

granit_result renderer_registry::copy_texture_to_buffer(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_texture source,
                                                        granit_buffer destination,
                                                        const granit_texture_data_layout& layout,
                                                        const granit_texture_write_region& region) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!recorder_record->transfers)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<texture_record> source_record;
  std::shared_ptr<buffer_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->owner;
    if (handles_.find(source, resource_type::texture, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = textures_.find(source);
    const auto found_destination = buffers_.find(destination);
    if (found_source == textures_.end() || found_destination == buffers_.end() ||
        found_source->second->owner != recorder_record->owner ||
        found_destination->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }

  const auto& desc = source_record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (destination_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer ||
      layout.offset % 4 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > destination_record->desc.size ||
      required > destination_record->desc.size - layout.offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (!recorder_record->commands->command_recorder_is_recording(*recorder_record->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->transfers->copy_texture_to_buffer(
      *recorder_record->native, *source_record->native, *destination_record->native, desc.format,
      layout, region);
}

granit_result renderer_registry::copy_buffer_to_texture(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_buffer source,
                                                        granit_texture destination,
                                                        const granit_texture_data_layout& layout,
                                                        const granit_texture_write_region& region) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!recorder_record->transfers)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<buffer_record> source_record;
  std::shared_ptr<texture_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->owner;
    if (handles_.find(source, resource_type::buffer, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::texture, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = buffers_.find(source);
    const auto found_destination = textures_.find(destination);
    if (found_source == buffers_.end() || found_destination == textures_.end() ||
        found_source->second->owner != state ||
        found_destination->second->owner != recorder_record->owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }

  const auto& desc = destination_record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((source_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer ||
      layout.offset % 4 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > source_record->desc.size ||
      required > source_record->desc.size - layout.offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (!recorder_record->commands->command_recorder_is_recording(*recorder_record->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->transfers->copy_buffer_to_texture(
      *recorder_record->native, *source_record->native, *destination_record->native, desc.format,
      layout, region);
}

granit_result renderer_registry::copy_texture(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              granit_texture source, granit_texture destination,
                                              const granit_texture_copy_region& region) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!recorder_record->transfers)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<texture_record> source_record;
  std::shared_ptr<texture_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->owner;
    if (handles_.find(source, resource_type::texture, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::texture, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = textures_.find(source);
    const auto found_destination = textures_.find(destination);
    if (found_source == textures_.end() || found_destination == textures_.end() ||
        found_source->second->owner != recorder_record->owner ||
        found_destination->second->owner != recorder_record->owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }

  const auto& source_desc = source_record->desc;
  const auto& destination_desc = destination_record->desc;
  if (source == destination || source_desc.format != destination_desc.format ||
      source_desc.sample_count != GRANIT_SAMPLE_COUNT_1 ||
      destination_desc.sample_count != GRANIT_SAMPLE_COUNT_1 || depth_format(source_desc.format) ||
      (source_desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (destination_desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.reserved != 0 ||
      region.width == 0 || region.height == 0 || region.depth == 0 ||
      region.array_layer_count == 0 || region.source_mip_level >= source_desc.mip_levels ||
      region.destination_mip_level >= destination_desc.mip_levels ||
      region.source_base_array_layer >= source_desc.array_layers ||
      region.destination_base_array_layer >= destination_desc.array_layers ||
      region.array_layer_count > source_desc.array_layers - region.source_base_array_layer ||
      region.array_layer_count >
          destination_desc.array_layers - region.destination_base_array_layer) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto region_fits = [](const granit_texture_desc& desc, std::uint32_t mip, std::uint32_t x,
                              std::uint32_t y, std::uint32_t z, std::uint32_t width,
                              std::uint32_t height, std::uint32_t depth) {
    const auto mip_width = std::max(UINT32_C(1), desc.width >> mip);
    const auto mip_height = std::max(UINT32_C(1), desc.height >> mip);
    const auto mip_depth = std::max(UINT32_C(1), desc.depth >> mip);
    return x < mip_width && width <= mip_width - x && y < mip_height && height <= mip_height - y &&
           z < mip_depth && depth <= mip_depth - z;
  };
  if (!region_fits(source_desc, region.source_mip_level, region.source_x, region.source_y,
                   region.source_z, region.width, region.height, region.depth) ||
      !region_fits(destination_desc, region.destination_mip_level, region.destination_x,
                   region.destination_y, region.destination_z, region.width, region.height,
                   region.depth)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (!recorder_record->commands->command_recorder_is_recording(*recorder_record->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->transfers->copy_texture(*recorder_record->native, *source_record->native,
                                                  *destination_record->native, region);
}

granit_result renderer_registry::generate_mipmaps(granit_renderer renderer,
                                                  granit_command_recorder recorder,
                                                  granit_texture texture,
                                                  const granit_texture_mipmap_range& range) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!recorder_record->transfers)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<texture_record> texture_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->owner;
    if (handles_.find(texture, resource_type::texture, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->owner != recorder_record->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    texture_record_state = found->second;
  }
  const auto& desc = texture_record_state->desc;
  if (depth_format(desc.format) || desc.sample_count != GRANIT_SAMPLE_COUNT_1 ||
      (desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      !recorder_record->transfers->texture_supports_linear_blit(desc.format)) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (range.level_count < 2 || range.array_layer_count == 0 ||
      range.base_mip_level >= desc.mip_levels ||
      range.level_count > desc.mip_levels - range.base_mip_level ||
      range.base_array_layer >= desc.array_layers ||
      range.array_layer_count > desc.array_layers - range.base_array_layer) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::lock_guard record_lock{recorder_record->mutex};
  if (!recorder_record->commands->command_recorder_is_recording(*recorder_record->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, texture_record_state,
                  texture_record_state->metadata);
  return recorder_record->transfers->generate_mipmaps(*recorder_record->native,
                                                      *texture_record_state->native, desc, range);
}

granit_result renderer_registry::fill_buffer(granit_renderer renderer,
                                             granit_command_recorder recorder, granit_buffer buffer,
                                             std::uint64_t offset, std::uint64_t size,
                                             std::uint32_t value) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (!recorder_record->transfers)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<buffer_record> buffer_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->owner;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    buffer_record_state = found->second;
  }
  if ((buffer_record_state->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      offset % 4 != 0 || size % 4 != 0 || size == 0 || offset >= buffer_record_state->desc.size ||
      size > buffer_record_state->desc.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (!recorder_record->commands->command_recorder_is_recording(*recorder_record->native)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  retain_resource(recorder_record->retained_resources, buffer_record_state,
                  buffer_record_state->metadata);
  return recorder_record->transfers->fill_buffer(*recorder_record->native,
                                                 *buffer_record_state->native, offset, size, value);
}

} // namespace granit::detail
