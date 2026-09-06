// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include <new>

namespace granit::detail {

void renderer_registry::poll_detached_async_operations(
    const std::shared_ptr<backend_renderer>& owner) {
  std::vector<std::shared_ptr<async_operation_record>> records;
  {
    std::lock_guard lock{mutex_};
    for (const auto& record : detached_async_operations_) {
      if (record->owner == owner)
        records.push_back(record);
    }
  }
  for (const auto& record : records) {
    if (record->poll)
      record->poll();
  }
  std::lock_guard lock{mutex_};
  std::erase_if(detached_async_operations_, [&](const auto& record) {
    if (record->owner != owner)
      return false;
    const auto status = record->state->status();
    return status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED ||
           status.state == GRANIT_ASYNC_OPERATION_STATE_FAILED ||
           status.state == GRANIT_ASYNC_OPERATION_STATE_CANCELLED;
  });
}

granit_result renderer_registry::register_async_operation(
    granit_renderer renderer, std::shared_ptr<async_operation_state_machine> state,
    granit_async_operation& operation, std::function<void()> poll, std::shared_ptr<void> payload,
    async_operation_kind kind) {
  if (!state)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<async_operation_record>();
    record->metadata.creation_sequence = next_creation_sequence_++;
    record->owner = owner->second;
    record->state = std::move(state);
    record->poll = std::move(poll);
    record->payload = std::move(payload);
    record->kind = kind;
    const auto handle =
        handles_.insert(record.get(), resource_type::async_operation, owner->second->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      async_operations_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(
          handles_.erase(handle, resource_type::async_operation, owner->second->domain()));
      throw;
    }
    operation = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_async_operation_status(granit_renderer renderer,
                                                            granit_async_operation operation,
                                                            granit_async_operation_status& status) {
  std::shared_ptr<async_operation_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end() ||
        handles_.find(operation, resource_type::async_operation, owner->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = async_operations_.find(operation);
    if (found == async_operations_.end() || found->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  if (record->poll)
    record->poll();
  status = record->state->status();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::request_async_operation_cancel(granit_renderer renderer,
                                                                granit_async_operation operation) {
  std::shared_ptr<async_operation_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end() ||
        handles_.find(operation, resource_type::async_operation, owner->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = async_operations_.find(operation);
    if (found == async_operations_.end() || found->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  static_cast<void>(record->state->request_cancel());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_async_operation(granit_renderer renderer,
                                                         granit_async_operation operation) {
  std::lock_guard lock{mutex_};
  const auto owner = backend_renderers_.find(renderer);
  if (owner == backend_renderers_.end() ||
      handles_.find(operation, resource_type::async_operation, owner->second->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = async_operations_.find(operation);
  if (found == async_operations_.end() || found->second->owner != owner->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  auto record = found->second;
  static_cast<void>(record->state->request_cancel());
  const auto status = record->state->status();
  if (status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING) {
    try {
      detached_async_operations_.push_back(record);
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
  }
  async_operations_.erase(found);
  return handles_.erase(operation, resource_type::async_operation, owner->second->domain());
}

} // namespace granit::detail
