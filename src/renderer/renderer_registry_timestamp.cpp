// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "renderer/renderer_registry_helpers.h"

#include <new>
#include <utility>

namespace granit::detail {

granit_result renderer_registry::create_timestamp_query_pool(granit_renderer renderer,
                                                             std::uint32_t query_count,
                                                             granit_timestamp_query_pool& pool) {
  try {
    auto owner = acquire_backend(renderer);
    if (!owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto timestamps = std::dynamic_pointer_cast<backend_timestamp_renderer>(owner);
    if (!timestamps)
      return GRANIT_ERROR_UNSUPPORTED;
    auto record = std::make_shared<timestamp_query_pool_record>();
    record->owner = owner;
    record->timestamps = timestamps;
    record->retirement = std::dynamic_pointer_cast<backend_retirement_renderer>(owner);
    const auto result = timestamps->create_timestamp_query_pool(query_count, record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::timestamp_query_pool, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      timestamp_query_pools_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(
          handles_.erase(handle, resource_type::timestamp_query_pool, owner->domain()));
      throw;
    }
    pool = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_timestamp_query_results(granit_renderer renderer,
                                                             granit_timestamp_query_pool pool,
                                                             std::uint32_t first,
                                                             std::span<std::uint64_t> nanoseconds) {
  std::shared_ptr<timestamp_query_pool_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(pool, resource_type::timestamp_query_pool,
                      found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  return record->timestamps->read_timestamp_query_results(*record->native, first, nanoseconds);
}

granit_result renderer_registry::destroy_timestamp_query_pool(granit_renderer renderer,
                                                              granit_timestamp_query_pool pool) {
  std::shared_ptr<timestamp_query_pool_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(pool, resource_type::timestamp_query_pool,
                      found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    timestamp_query_pools_.erase(found);
    static_cast<void>(handles_.erase(pool, resource_type::timestamp_query_pool,
                                     found_renderer->second->domain()));
  }
  const auto retirement = record->retirement;
  const auto serial = record->metadata.last_use_serial.load();
  if (retirement) {
    retirement->retire_resource(serial, retirement_order::dependent, std::move(record));
    static_cast<void>(retirement->collect_retired());
  } else {
    record.reset();
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::reset_timestamp_queries(granit_renderer renderer,
                                                         granit_command_recorder recorder,
                                                         granit_timestamp_query_pool pool,
                                                         std::uint32_t first, std::uint32_t count) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!command->timestamps)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<timestamp_query_pool_record> query;
  {
    std::lock_guard lock{mutex_};
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->owner != command->owner ||
        handles_.find(pool, resource_type::timestamp_query_pool, command->owner->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    query = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (!command->commands->command_recorder_is_recording(*command->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard query_lock{query->mutex};
  const auto result =
      command->timestamps->reset_timestamp_queries(*command->native, *query->native, first, count);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, query, query->metadata);
  return result;
}

granit_result renderer_registry::write_timestamp(granit_renderer renderer,
                                                 granit_command_recorder recorder,
                                                 granit_timestamp_query_pool pool,
                                                 granit_timestamp_stage stage,
                                                 std::uint32_t index) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!command->timestamps)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<timestamp_query_pool_record> query;
  {
    std::lock_guard lock{mutex_};
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->owner != command->owner ||
        handles_.find(pool, resource_type::timestamp_query_pool, command->owner->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    query = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (!command->commands->command_recorder_is_recording(*command->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard query_lock{query->mutex};
  const auto result =
      command->timestamps->write_timestamp(*command->native, *query->native, stage, index);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, query, query->metadata);
  return result;
}

} // namespace granit::detail
