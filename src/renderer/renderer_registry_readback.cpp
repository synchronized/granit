// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include <cstring>
#include <limits>
#include <new>

namespace granit::detail {
namespace {

bool exceeds_capacity(std::uint64_t current, std::uint64_t added, std::uint64_t maximum) {
  return maximum != 0 && (current > maximum || added > maximum - current);
}

} // namespace

granit_result renderer_registry::create_readback_batch(granit_renderer renderer,
                                                       const granit_readback_batch_desc& desc,
                                                       granit_readback_batch& batch) {
  try {
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (!interfaces->resources)
      return GRANIT_ERROR_UNSUPPORTED;
    auto record = std::make_shared<readback_batch_record>();
    record->owner = interfaces->renderer;
    record->resource_api = interfaces->resources;
    record->max_result_bytes = desc.max_result_bytes;
    record->max_operation_count = desc.max_operation_count;
    record->texture_layout = desc.texture_layout;
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end() || owner->second != record->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::readback_batch, record->owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      readback_batches_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(
          handles_.erase(handle, resource_type::readback_batch, owner->second->domain()));
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

granit_result
renderer_registry::readback_batch_read_buffer(granit_renderer renderer, granit_readback_batch batch,
                                              granit_buffer buffer, std::uint64_t offset,
                                              std::uint64_t size, std::uint32_t& result_index) {
  std::shared_ptr<readback_batch_record> batch_record;
  std::shared_ptr<buffer_record> buffer_record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end() ||
        handles_.find(batch, resource_type::readback_batch, owner->second->domain()) == nullptr ||
        handles_.find(buffer, resource_type::buffer, owner->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found_batch = readback_batches_.find(batch);
    const auto found_buffer = buffers_.find(buffer);
    if (found_batch == readback_batches_.end() || found_buffer == buffers_.end() ||
        found_batch->second->owner != owner->second || found_buffer->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    batch_record = found_batch->second;
    buffer_record = found_buffer->second;
  }
  std::scoped_lock locks{batch_record->mutex, buffer_record->mutex};
  if (batch_record->failed || size == 0 || offset > buffer_record->desc.size ||
      size > buffer_record->desc.size - offset || buffer_record->mapped ||
      (buffer_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((batch_record->max_result_bytes != 0 && size > batch_record->max_result_bytes) ||
      (batch_record->max_operation_count != 0 && batch_record->max_operation_count < 1))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (exceeds_capacity(batch_record->result_bytes, size, batch_record->max_result_bytes) ||
      (batch_record->max_operation_count != 0 &&
       batch_record->readbacks.size() >= batch_record->max_operation_count))
    return GRANIT_ERROR_NOT_READY;
  if (batch_record->readbacks.size() >= UINT32_MAX)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  try {
    result_index = static_cast<std::uint32_t>(batch_record->readbacks.size());
    granit_readback_result_info info = GRANIT_READBACK_RESULT_INFO_INIT;
    info.type = GRANIT_READBACK_RESULT_TYPE_BUFFER;
    info.required_size = size;
    batch_record->readbacks.push_back({.type = backend_readback_type::buffer,
                                       .buffer = buffer_record,
                                       .texture = {},
                                       .offset = offset,
                                       .size = size,
                                       .texture_region = {},
                                       .result_info = info});
    batch_record->result_bytes += size;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::readback_batch_read_texture(
    granit_renderer renderer, granit_readback_batch batch, granit_texture texture,
    const granit_texture_write_region& region, std::uint32_t& result_index) {
  std::shared_ptr<readback_batch_record> batch_record;
  std::shared_ptr<texture_record> texture_record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end() ||
        handles_.find(batch, resource_type::readback_batch, owner->second->domain()) == nullptr ||
        handles_.find(texture, resource_type::texture, owner->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found_batch = readback_batches_.find(batch);
    const auto found_texture = textures_.find(texture);
    if (found_batch == readback_batches_.end() || found_texture == textures_.end() ||
        found_batch->second->owner != owner->second ||
        found_texture->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    batch_record = found_batch->second;
    texture_record = found_texture->second;
  }
  granit_texture_readback_info texture_info = GRANIT_TEXTURE_READBACK_INFO_INIT;
  const auto info_result = get_texture_readback_info(renderer, texture, region, texture_info);
  if (info_result != GRANIT_SUCCESS)
    return info_result;
  std::lock_guard lock{batch_record->mutex};
  const auto size = texture_info.required_size;
  if (batch_record->failed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((batch_record->max_result_bytes != 0 && size > batch_record->max_result_bytes) ||
      (batch_record->max_operation_count != 0 && batch_record->max_operation_count < 1))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (exceeds_capacity(batch_record->result_bytes, size, batch_record->max_result_bytes) ||
      (batch_record->max_operation_count != 0 &&
       batch_record->readbacks.size() >= batch_record->max_operation_count))
    return GRANIT_ERROR_NOT_READY;
  if (batch_record->readbacks.size() >= UINT32_MAX)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  try {
    result_index = static_cast<std::uint32_t>(batch_record->readbacks.size());
    granit_readback_result_info info = GRANIT_READBACK_RESULT_INFO_INIT;
    info.type = GRANIT_READBACK_RESULT_TYPE_TEXTURE;
    info.required_size = size;
    info.format = texture_info.format;
    info.width = texture_info.width;
    info.height = texture_info.height;
    info.depth = texture_info.depth;
    info.array_layer_count = texture_info.array_layer_count;
    info.bytes_per_row = texture_info.bytes_per_row;
    info.rows_per_image = texture_info.rows_per_image;
    batch_record->readbacks.push_back({.type = backend_readback_type::texture,
                                       .buffer = {},
                                       .texture = texture_record,
                                       .offset = 0,
                                       .size = size,
                                       .texture_region = region,
                                       .result_info = info});
    batch_record->result_bytes += size;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_readback_batch_info(granit_renderer renderer,
                                                         granit_readback_batch batch,
                                                         granit_readback_batch_info& info) {
  std::shared_ptr<readback_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = readback_batches_.find(batch);
    if (owner == backend_renderers_.end() || found == readback_batches_.end() ||
        found->second->owner != owner->second ||
        handles_.find(batch, resource_type::readback_batch, owner->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  info.operation_count = static_cast<std::uint32_t>(record->readbacks.size());
  info.result_bytes = record->result_bytes;
  info.max_operation_count = record->max_operation_count;
  info.reserved = 0;
  info.max_result_bytes = record->max_result_bytes;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::submit_readback_batch_async(granit_renderer renderer,
                                                             granit_readback_batch batch,
                                                             granit_async_operation& operation) {
  operation = GRANIT_NULL_HANDLE;
  std::shared_ptr<readback_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = readback_batches_.find(batch);
    if (owner == backend_renderers_.end() || found == readback_batches_.end() ||
        found->second->owner != owner->second ||
        handles_.find(batch, resource_type::readback_batch, owner->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard batch_lock{record->mutex};
  if (record->failed || record->readbacks.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto state = std::make_shared<async_operation_state_machine>();
    auto payload = std::make_shared<readback_batch_operation>();
    std::vector<backend_readback_operation> readbacks;
    readbacks.reserve(record->readbacks.size());
    for (const auto& entry : record->readbacks) {
      readbacks.push_back({.type = entry.type,
                           .buffer = entry.buffer ? entry.buffer->native.get() : nullptr,
                           .texture = entry.texture ? entry.texture->native.get() : nullptr,
                           .source_offset = entry.offset,
                           .size = entry.size,
                           .format = entry.result_info.format,
                           .texture_region = entry.texture_region,
                           .result_info = entry.result_info});
    }
    payload->readbacks = std::move(record->readbacks);
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
                                           async_operation_kind::readback_batch);
    if (result != GRANIT_SUCCESS) {
      record->readbacks = std::move(payload->readbacks);
      return result;
    }
    result = record->resource_api->readback_batch_async(
        readbacks, record->texture_layout, record->max_result_bytes, payload->completion);
    if (result != GRANIT_SUCCESS) {
      record->readbacks = std::move(payload->readbacks);
      record->failed = true;
      static_cast<void>(destroy_async_operation(renderer, operation));
      operation = GRANIT_NULL_HANDLE;
      return result;
    }
    record->result_bytes = 0;
    static_cast<void>(state->begin());
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_readback_result_info(granit_renderer renderer,
                                                          granit_async_operation operation,
                                                          std::uint32_t result_index,
                                                          granit_readback_result_info& info) {
  std::shared_ptr<async_operation_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = async_operations_.find(operation);
    if (owner == backend_renderers_.end() || found == async_operations_.end() ||
        found->second->owner != owner->second ||
        found->second->kind != async_operation_kind::readback_batch)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  if (record->poll)
    record->poll();
  const auto status = record->state->status();
  if (status.state == GRANIT_ASYNC_OPERATION_STATE_FAILED ||
      status.state == GRANIT_ASYNC_OPERATION_STATE_CANCELLED)
    return status.result;
  if (status.state != GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED)
    return GRANIT_ERROR_NOT_READY;
  const auto payload = std::static_pointer_cast<readback_batch_operation>(record->payload);
  if (!payload || result_index >= payload->readbacks.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard lock{payload->mutex};
  return payload->completion->get_result_info(result_index, info);
}

granit_result renderer_registry::copy_readback_result(granit_renderer renderer,
                                                      granit_async_operation operation,
                                                      std::uint32_t result_index, void* data,
                                                      std::uint64_t& size) {
  granit_readback_result_info info = GRANIT_READBACK_RESULT_INFO_INIT;
  const auto info_result = get_readback_result_info(renderer, operation, result_index, info);
  if (info_result != GRANIT_SUCCESS)
    return info_result;
  const auto capacity = size;
  size = info.required_size;
  if (data == nullptr)
    return capacity == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
  if (capacity < info.required_size)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::shared_ptr<async_operation_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = async_operations_.find(operation);
    if (found == async_operations_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  const auto payload = std::static_pointer_cast<readback_batch_operation>(record->payload);
  std::lock_guard lock{payload->mutex};
  return payload->completion->copy_result(result_index, data, info.required_size);
}

granit_result renderer_registry::reset_readback_batch(granit_renderer renderer,
                                                      granit_readback_batch batch) {
  std::shared_ptr<readback_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = readback_batches_.find(batch);
    if (owner == backend_renderers_.end() || found == readback_batches_.end() ||
        found->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  record->readbacks.clear();
  record->result_bytes = 0;
  record->failed = false;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_readback_batch(granit_renderer renderer,
                                                        granit_readback_batch batch) {
  std::lock_guard lock{mutex_};
  const auto owner = backend_renderers_.find(renderer);
  const auto found = readback_batches_.find(batch);
  if (owner == backend_renderers_.end() || found == readback_batches_.end() ||
      found->second->owner != owner->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto result = handles_.erase(batch, resource_type::readback_batch, owner->second->domain());
  if (result == GRANIT_SUCCESS)
    readback_batches_.erase(found);
  return result;
}

} // namespace granit::detail
