// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "core/texture_format.h"
#include "renderer/renderer_registry_helpers.h"

#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result renderer_registry::create_buffer(granit_renderer renderer,
                                               const granit_buffer_desc& desc,
                                               granit_buffer& buffer) {
  try {
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& owner = interfaces->renderer;
    const auto& resource_api = interfaces->resources;
    if (!resource_api)
      return GRANIT_ERROR_UNSUPPORTED;
    auto record = std::make_shared<buffer_record>();
    record->owner = owner;
    record->resource_api = resource_api;
    record->retirement = interfaces->retirement;
    record->desc = desc;
    record->native = resource_api->allocate_buffer_resource();
    const auto create_result = resource_api->create_buffer(desc, *record->native);
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() || renderer_found->second != owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::buffer, owner->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      buffers_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::buffer, owner->domain()));
      throw;
    }
    buffer = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::map_buffer(granit_renderer renderer, granit_buffer buffer,
                                            std::uint64_t offset, std::uint64_t size, void*& data) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (record->mapped) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (record->desc.memory_location != GRANIT_MEMORY_LOCATION_UPLOAD &&
      record->desc.memory_location != GRANIT_MEMORY_LOCATION_READBACK) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (offset >= record->desc.size || size == 0 || size > record->desc.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_READBACK) {
    const auto result = record->resource_api->invalidate_buffer(*record->native, offset, size);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record->mapped = true;
  record->mapped_offset = offset;
  record->mapped_size = size;
  data = static_cast<unsigned char*>(record->resource_api->mapped_buffer_data(*record->native)) +
         offset;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::get_buffer_desc(granit_renderer renderer, granit_buffer buffer,
                                                 granit_buffer_desc& desc) {
  std::lock_guard lock{mutex_};
  const auto renderer_found = backend_renderers_.find(renderer);
  if (renderer_found == backend_renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& state = renderer_found->second;
  if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = buffers_.find(buffer);
  if (found == buffers_.end() || found->second->owner != state)
    return GRANIT_ERROR_INVALID_HANDLE;
  desc = found->second->desc;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::unmap_buffer(granit_renderer renderer, granit_buffer buffer) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (!record->mapped) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_result result = GRANIT_SUCCESS;
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_UPLOAD) {
    result = record->resource_api->flush_buffer(*record->native, record->mapped_offset,
                                                record->mapped_size);
  }
  record->mapped = false;
  record->mapped_offset = 0;
  record->mapped_size = 0;
  return result;
}

granit_result renderer_registry::flush_mapped_buffer(granit_renderer renderer, granit_buffer buffer,
                                                     std::uint64_t offset, std::uint64_t size) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (!record->mapped || record->desc.memory_location != GRANIT_MEMORY_LOCATION_UPLOAD ||
      size == 0 || offset < record->mapped_offset ||
      offset > record->mapped_offset + record->mapped_size ||
      size > record->mapped_offset + record->mapped_size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return record->resource_api->flush_buffer(*record->native, offset, size);
}

granit_result renderer_registry::destroy_buffer(granit_renderer renderer, granit_buffer buffer) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    {
      std::lock_guard record_lock{found->second->mutex};
      if (found->second->mapped) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    record = std::move(found->second);
    buffers_.erase(found);
    const auto erase_result = handles_.erase(buffer, resource_type::buffer, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  const auto retirement = record->retirement;
  const auto serial = record->metadata.last_use_serial.load();
  if (retirement) {
    retirement->retire_resource(serial, retirement_order::resource, std::move(record));
    static_cast<void>(retirement->collect_retired());
  } else {
    record.reset();
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::write_buffer(granit_renderer renderer, granit_buffer buffer,
                                              std::uint64_t offset, const void* data,
                                              std::uint64_t size) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (record->mapped || size == 0 || offset >= record->desc.size ||
      size > record->desc.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_READBACK) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_UPLOAD) {
    std::memcpy(
        static_cast<unsigned char*>(record->resource_api->mapped_buffer_data(*record->native)) +
            offset,
        data, static_cast<std::size_t>(size));
    return record->resource_api->flush_buffer(*record->native, offset, size);
  }
  return record->resource_api->upload_buffer(*record->native, offset, data, size);
}

granit_result renderer_registry::create_upload_batch(granit_renderer renderer,
                                                     granit_upload_batch& batch) {
  try {
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = interfaces->renderer;
    const auto& resource_api = interfaces->resources;
    if (!resource_api)
      return GRANIT_ERROR_UNSUPPORTED;
    auto record = std::make_shared<upload_batch_record>();
    record->owner = owner;
    record->resource_api = resource_api;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::upload_batch, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      upload_batches_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::upload_batch, owner->domain()));
      throw;
    }
    batch = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::upload_batch_write_buffer(granit_renderer renderer,
                                                           granit_upload_batch batch,
                                                           granit_buffer buffer,
                                                           std::uint64_t offset, const void* data,
                                                           std::uint64_t size) {
  std::shared_ptr<upload_batch_record> batch_record;
  std::shared_ptr<buffer_record> buffer_record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(batch, resource_type::upload_batch, state->domain()) == nullptr ||
        handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found_batch = upload_batches_.find(batch);
    const auto found_buffer = buffers_.find(buffer);
    if (found_batch == upload_batches_.end() || found_buffer == buffers_.end() ||
        found_batch->second->owner != state || found_buffer->second->owner != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    batch_record = found_batch->second;
    buffer_record = found_buffer->second;
  }

  std::scoped_lock record_locks{batch_record->mutex, buffer_record->mutex};
  if (batch_record->failed || size > SIZE_MAX || buffer_record->mapped ||
      offset >= buffer_record->desc.size || size > buffer_record->desc.size - offset ||
      (offset & 3) != 0 || (size & 3) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (buffer_record->desc.memory_location != GRANIT_MEMORY_LOCATION_DEVICE &&
      buffer_record->desc.memory_location != GRANIT_MEMORY_LOCATION_AUTOMATIC)
    return GRANIT_ERROR_UNSUPPORTED;
  try {
    upload_entry entry{.type = backend_upload_type::buffer,
                       .buffer = buffer_record,
                       .texture = {},
                       .offset = offset,
                       .data = {},
                       .texture_copy = {}};
    entry.data.resize(static_cast<std::size_t>(size));
    std::memcpy(entry.data.data(), data, static_cast<std::size_t>(size));
    batch_record->uploads.push_back(std::move(entry));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::upload_batch_write_texture(
    granit_renderer renderer, granit_upload_batch batch, granit_texture texture, const void* data,
    std::uint64_t size, const granit_texture_data_layout& layout,
    const granit_texture_write_region& region) {
  std::shared_ptr<upload_batch_record> batch_record;
  std::shared_ptr<texture_record> texture_record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(batch, resource_type::upload_batch, state->domain()) == nullptr ||
        handles_.find(texture, resource_type::texture, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found_batch = upload_batches_.find(batch);
    const auto found_texture = textures_.find(texture);
    if (found_batch == upload_batches_.end() || found_texture == textures_.end() ||
        found_batch->second->owner != state || found_texture->second->owner != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    batch_record = found_batch->second;
    texture_record = found_texture->second;
  }

  std::scoped_lock record_locks{batch_record->mutex, texture_record->mutex};
  if (batch_record->failed || size > SIZE_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& desc = texture_record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z)
    return GRANIT_ERROR_INVALID_ARGUMENT;

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
      region.height - 1 > max / row_pitch)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > size || required > size - layout.offset || required > SIZE_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  const backend_texture_copy copy{
      .buffer_row_length = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel,
      .buffer_image_height = layout.rows_per_image,
      .aspect = region.aspect,
      .mip_level = region.mip_level,
      .base_array_layer = region.base_array_layer,
      .array_layer_count = region.array_layer_count,
      .x = static_cast<std::int32_t>(region.x),
      .y = static_cast<std::int32_t>(region.y),
      .z = static_cast<std::int32_t>(region.z),
      .width = region.width,
      .height = region.height,
      .depth = region.depth,
  };
  try {
    upload_entry entry{.type = backend_upload_type::texture,
                       .buffer = {},
                       .texture = texture_record,
                       .offset = 0,
                       .data = {},
                       .texture_copy = copy};
    entry.data.resize(static_cast<std::size_t>(required));
    std::memcpy(entry.data.data(), static_cast<const std::byte*>(data) + layout.offset,
                static_cast<std::size_t>(required));
    batch_record->uploads.push_back(std::move(entry));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::submit_upload_batch(granit_renderer renderer,
                                                     granit_upload_batch batch) {
  std::shared_ptr<upload_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
        handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = upload_batches_.find(batch);
    if (found == upload_batches_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard batch_lock{record->mutex};
  if (record->failed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (record->uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<backend_upload_operation> uploads;
  uploads.reserve(record->uploads.size());
  for (const auto& upload : record->uploads) {
    uploads.push_back({.type = upload.type,
                       .buffer = upload.buffer ? upload.buffer->native.get() : nullptr,
                       .texture = upload.texture ? upload.texture->native.get() : nullptr,
                       .destination_offset = upload.offset,
                       .data = upload.data.data(),
                       .size = upload.data.size(),
                       .texture_copy = upload.texture_copy});
  }
  const auto result = record->resource_api->upload_batch(uploads);
  if (result == GRANIT_SUCCESS)
    record->uploads.clear();
  else
    record->failed = true;
  return result;
}

granit_result renderer_registry::reset_upload_batch(granit_renderer renderer,
                                                    granit_upload_batch batch) {
  std::shared_ptr<upload_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
        handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = upload_batches_.find(batch);
    if (found == upload_batches_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  record->uploads.clear();
  record->failed = false;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_upload_batch(granit_renderer renderer,
                                                      granit_upload_batch batch) {
  std::lock_guard lock{mutex_};
  const auto found_renderer = backend_renderers_.find(renderer);
  if (found_renderer == backend_renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
      handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
          nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = upload_batches_.find(batch);
  if (found == upload_batches_.end() || found->second->owner != found_renderer->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto result =
      handles_.erase(batch, resource_type::upload_batch, found_renderer->second->domain());
  if (result != GRANIT_SUCCESS)
    return result;
  upload_batches_.erase(found);
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
