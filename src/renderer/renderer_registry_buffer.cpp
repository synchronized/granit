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
                                                     std::uint64_t max_staged_bytes,
                                                     std::uint32_t max_operation_count,
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
    record->max_staged_bytes = max_staged_bytes;
    record->max_operation_count = max_operation_count;
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
  if ((batch_record->max_staged_bytes != 0 && size > batch_record->max_staged_bytes) ||
      (batch_record->max_operation_count != 0 && batch_record->max_operation_count < 1))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((batch_record->max_staged_bytes != 0 &&
       size > batch_record->max_staged_bytes - batch_record->staged_bytes) ||
      (batch_record->max_operation_count != 0 &&
       batch_record->uploads.size() >= batch_record->max_operation_count))
    return GRANIT_ERROR_NOT_READY;
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
    batch_record->staged_bytes += size;
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
  const auto bytes_per_block =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_block == 0)
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
  if (!texture_region_has_valid_block_alignment(desc.format, region.x, region.y, region.width,
                                                region.height, mip_width, mip_height))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  texture_transfer_footprint transfer{};
  if (!calculate_texture_transfer_footprint(desc.format, region.width, region.height, image_count,
                                            layout, transfer))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto required = transfer.required_size;
  if (layout.offset > size || required > size - layout.offset || required > SIZE_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((batch_record->max_staged_bytes != 0 && required > batch_record->max_staged_bytes) ||
      (batch_record->max_operation_count != 0 && batch_record->max_operation_count < 1))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((batch_record->max_staged_bytes != 0 &&
       required > batch_record->max_staged_bytes - batch_record->staged_bytes) ||
      (batch_record->max_operation_count != 0 &&
       batch_record->uploads.size() >= batch_record->max_operation_count))
    return GRANIT_ERROR_NOT_READY;

  const auto block = texture_format_block(desc.format);
  if (layout.bytes_per_row != 0 && layout.bytes_per_row / block.bytes > UINT32_MAX / block.width)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (layout.rows_per_image != 0 && layout.rows_per_image > UINT32_MAX / block.height)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const backend_texture_copy copy{
      .buffer_row_length =
          layout.bytes_per_row == 0 ? 0 : (layout.bytes_per_row / block.bytes) * block.width,
      .buffer_image_height = layout.rows_per_image == 0 ? 0 : layout.rows_per_image * block.height,
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
    batch_record->staged_bytes += required;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::get_upload_batch_info(granit_renderer renderer,
                                                       granit_upload_batch batch,
                                                       granit_upload_batch_info& info) {
  std::shared_ptr<upload_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = upload_batches_.find(batch);
    if (found == upload_batches_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  info.staged_bytes = record->staged_bytes;
  info.operation_count = static_cast<std::uint32_t>(record->uploads.size());
  info.max_staged_bytes = record->max_staged_bytes;
  info.max_operation_count = record->max_operation_count;
  return GRANIT_SUCCESS;
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
  if (result == GRANIT_SUCCESS) {
    record->uploads.clear();
    record->staged_bytes = 0;
  } else {
    record->failed = true;
  }
  return result;
}

granit_result renderer_registry::submit_upload_batch_async(granit_renderer renderer,
                                                           granit_upload_batch batch,
                                                           granit_async_operation& operation) {
  operation = GRANIT_NULL_HANDLE;
  std::shared_ptr<upload_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = upload_batches_.find(batch);
    if (found == upload_batches_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }

  std::lock_guard batch_lock{record->mutex};
  if (record->failed || record->uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto state = std::make_shared<async_operation_state_machine>();
    auto payload = std::make_shared<upload_batch_operation>();
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
    payload->uploads = std::move(record->uploads);
    const auto poll = [state, payload] {
      if (state->status().state != GRANIT_ASYNC_OPERATION_STATE_RUNNING)
        return;
      std::lock_guard lock{payload->mutex};
      if (!payload->completion)
        return;
      const auto result = payload->completion->poll();
      if (result != GRANIT_ERROR_NOT_READY)
        state->complete(result);
    };
    auto result = register_async_operation(renderer, state, operation, poll, payload,
                                           async_operation_kind::upload_batch);
    if (result != GRANIT_SUCCESS) {
      record->uploads = std::move(payload->uploads);
      return result;
    }
    result = record->resource_api->upload_batch_async(uploads, payload->completion);
    if (result != GRANIT_SUCCESS) {
      record->uploads = std::move(payload->uploads);
      record->failed = result != GRANIT_ERROR_NOT_READY;
      state->complete(result);
      static_cast<void>(destroy_async_operation(renderer, operation));
      operation = GRANIT_NULL_HANDLE;
      return result;
    }
    record->staged_bytes = 0;
    static_cast<void>(state->begin());
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
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
  record->staged_bytes = 0;
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
