// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "renderer/renderer_registry_helpers.h"

#include <new>
#include <utility>

namespace granit::detail {

granit_result renderer_registry::create_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder& recorder) {
  try {
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& owner = interfaces->renderer;
    auto record = std::make_shared<command_recorder_record>();
    record->owner = owner;
    record->queue = interfaces->queue;
    record->commands = interfaces->commands;
    record->compute = interfaces->compute;
    record->graphics = interfaces->graphics;
    record->retirement = interfaces->retirement;
    record->timestamps = interfaces->timestamps;
    record->transfers = interfaces->transfer;
    if (!record->queue || !record->commands || !record->graphics)
      return GRANIT_ERROR_INTERNAL;
    record->native = record->commands->allocate_command_recorder_resource();
    const auto result = record->native ? record->commands->create_command_recorder(*record->native)
                                       : GRANIT_ERROR_OUT_OF_MEMORY;
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::command_recorder, owner->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      command_recorders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::command_recorder, owner->domain()));
      throw;
    }
    recorder = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::begin_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  return record->commands->begin_command_recorder(*record->native);
}

granit_result renderer_registry::end_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  return record->commands->end_command_recorder(*record->native);
}

granit_result renderer_registry::submit_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard record_lock{record->mutex};
  submission_serial serial{};
  const auto result = record->queue->submit_command_recorder(*record->native, serial);
  if (result == GRANIT_SUCCESS)
    mark_resources_used(record->retained_resources, serial);
  return result;
}

granit_result
renderer_registry::submit_command_recorders(granit_renderer renderer,
                                            std::span<const granit_command_recorder> recorders) {
  if (recorders.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<std::shared_ptr<command_recorder_record>> records;
  records.reserve(recorders.size());
  std::shared_ptr<backend_renderer> owner;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    owner = found_renderer->second;
    for (const auto handle : recorders) {
      if (handle == GRANIT_NULL_HANDLE ||
          handles_.find(handle, resource_type::command_recorder, owner->domain()) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      const auto found = command_recorders_.find(handle);
      if (found == command_recorders_.end() || found->second->owner != owner)
        return GRANIT_ERROR_INVALID_HANDLE;
      if (std::find(records.begin(), records.end(), found->second) != records.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      records.push_back(found->second);
    }
  }
  std::vector<command_recorder_record*> lock_order;
  lock_order.reserve(records.size());
  for (const auto& record : records)
    lock_order.push_back(record.get());
  std::sort(lock_order.begin(), lock_order.end(), std::less<>{});
  std::vector<std::unique_lock<std::mutex>> record_locks;
  record_locks.reserve(lock_order.size());
  for (auto* record : lock_order)
    record_locks.emplace_back(record->mutex);
  std::vector<backend_command_recorder_resource*> native_recorders;
  native_recorders.reserve(records.size());
  for (const auto& record : records)
    native_recorders.push_back(record->native.get());
  submission_serial serial{};
  const auto result = records.front()->queue->submit_command_recorders(native_recorders, serial);
  if (result == GRANIT_SUCCESS) {
    for (const auto& record : records)
      mark_resources_used(record->retained_resources, serial);
  }
  return result;
}

granit_result renderer_registry::reset_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  const auto wait_result = record->queue->wait_command_recorder(*record->native);
  if (wait_result != GRANIT_SUCCESS) {
    return wait_result;
  }
  const auto result = record->commands->reset_command_recorder(*record->native);
  if (result == GRANIT_SUCCESS) {
    record->retained_resources.clear();
    if (record->retirement)
      static_cast<void>(record->retirement->collect_retired());
  }
  return result;
}

granit_result renderer_registry::create_frame_context(granit_renderer renderer,
                                                      granit_frame_context& context) {
  const auto interfaces = acquire_backend_interfaces(renderer);
  if (!interfaces)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& owner = interfaces->renderer;
  const auto& presentation = interfaces->presentation;
  if (!presentation)
    return GRANIT_ERROR_UNSUPPORTED;
  auto record = std::make_shared<frame_context_record>();
  record->owner = owner;
  record->slots.resize(presentation->frame_slot_count());
  for (auto& slot : record->slots) {
    const auto result = create_command_recorder(renderer, slot.recorder);
    if (result != GRANIT_SUCCESS) {
      for (auto& created : record->slots) {
        if (created.recorder != GRANIT_NULL_HANDLE)
          static_cast<void>(destroy_command_recorder(renderer, created.recorder));
      }
      return result;
    }
  }
  granit_result install_result{GRANIT_SUCCESS};
  {
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner) {
      install_result = GRANIT_ERROR_INVALID_HANDLE;
    } else {
      record->metadata.creation_sequence = next_creation_sequence_++;
      const auto handle =
          handles_.insert(record.get(), resource_type::frame_context, owner->domain());
      if (handle == GRANIT_NULL_HANDLE) {
        install_result = GRANIT_ERROR_OUT_OF_MEMORY;
      } else {
        for (const auto& slot : record->slots)
          command_recorders_.at(slot.recorder)->owned_by_frame_context = true;
        try {
          frame_contexts_.emplace(handle, record);
          context = handle;
        } catch (...) {
          static_cast<void>(handles_.erase(handle, resource_type::frame_context, owner->domain()));
          for (const auto& slot : record->slots)
            command_recorders_.at(slot.recorder)->owned_by_frame_context = false;
          install_result = GRANIT_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }
  if (install_result != GRANIT_SUCCESS) {
    for (const auto& slot : record->slots)
      static_cast<void>(destroy_command_recorder(renderer, slot.recorder));
  }
  return install_result;
}

granit_result renderer_registry::begin_frame_context(granit_renderer renderer,
                                                     granit_frame_context context,
                                                     granit_frame frame,
                                                     granit_command_recorder& recorder,
                                                     std::uint32_t& frame_slot) {
  std::shared_ptr<frame_context_record> context_record;
  std::shared_ptr<frame_record> frame_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    const auto context_found = frame_contexts_.find(context);
    const auto frame_found = frames_.find(frame);
    if (renderer_found == backend_renderers_.end() || context_found == frame_contexts_.end() ||
        frame_found == frames_.end() || context_found->second->owner != renderer_found->second ||
        frame_found->second->owner != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    context_record = context_found->second;
    frame_record_state = frame_found->second;
  }
  std::size_t slot_index{};
  {
    std::lock_guard frame_lock{frame_record_state->mutex};
    if (frame_record_state->submitted)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    slot_index = frame_record_state->slot_index;
  }
  std::lock_guard context_lock{context_record->mutex};
  if (slot_index >= context_record->slots.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = context_record->slots[slot_index];
  if (slot.state == frame_context_slot_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (slot.state == frame_context_slot_state::submitted) {
    const auto reset_result = reset_command_recorder(renderer, slot.recorder);
    if (reset_result != GRANIT_SUCCESS)
      return reset_result;
    slot.state = frame_context_slot_state::idle;
    for (auto& page : slot.transient_pages)
      page.offset = 0;
  }
  const auto begin_result = begin_command_recorder(renderer, slot.recorder);
  if (begin_result != GRANIT_SUCCESS)
    return begin_result;
  slot.frame = frame;
  slot.state = frame_context_slot_state::recording;
  recorder = slot.recorder;
  frame_slot = static_cast<std::uint32_t>(slot_index);
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::allocate_transient_buffer(granit_renderer renderer,
                                                           granit_frame_context context,
                                                           granit_frame frame,
                                                           const granit_transient_buffer_desc& desc,
                                                           granit_transient_buffer_slice& slice) {
  constexpr granit_buffer_usage supported_usage =
      GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT | GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT |
      GRANIT_BUFFER_USAGE_VERTEX_BIT | GRANIT_BUFFER_USAGE_INDEX_BIT |
      GRANIT_BUFFER_USAGE_UNIFORM_BIT | GRANIT_BUFFER_USAGE_STORAGE_BIT |
      GRANIT_BUFFER_USAGE_INDIRECT_BIT;
  if ((desc.usage & ~supported_usage) != 0 ||
      (desc.memory_location != GRANIT_MEMORY_LOCATION_AUTOMATIC &&
       desc.memory_location != GRANIT_MEMORY_LOCATION_DEVICE))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::shared_ptr<frame_context_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    const auto context_found = frame_contexts_.find(context);
    if (renderer_found == backend_renderers_.end() || context_found == frame_contexts_.end() ||
        context_found->second->owner != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = context_found->second;
  }

  std::lock_guard context_lock{record->mutex};
  auto slot = std::find_if(record->slots.begin(), record->slots.end(), [&](const auto& item) {
    return item.state == frame_context_slot_state::recording && item.frame == frame;
  });
  if (slot == record->slots.end())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  auto alignment = std::max<std::uint64_t>(desc.alignment, 4);
  const auto& capabilities = record->owner->capabilities();
  if ((desc.usage & GRANIT_BUFFER_USAGE_UNIFORM_BIT) != 0)
    alignment = std::max(alignment, capabilities.uniform_buffer_offset_alignment);
  if ((desc.usage & GRANIT_BUFFER_USAGE_STORAGE_BIT) != 0)
    alignment = std::max(alignment, capabilities.storage_buffer_offset_alignment);
  const auto align_up = [alignment](std::uint64_t value, std::uint64_t& aligned) {
    const auto mask = alignment - 1;
    if (value > UINT64_MAX - mask)
      return false;
    aligned = (value + mask) & ~mask;
    return true;
  };

  for (auto& page : slot->transient_pages) {
    if (page.usage != desc.usage || page.memory_location != desc.memory_location)
      continue;
    std::uint64_t offset{};
    if (!align_up(page.offset, offset) || offset > page.size || desc.size > page.size - offset)
      continue;
    page.offset = offset + desc.size;
    slice = {.buffer = page.buffer,
             .offset = offset,
             .size = desc.size,
             .usage = desc.usage,
             .reserved = 0};
    return GRANIT_SUCCESS;
  }

  std::uint64_t minimum_size{};
  if (!align_up(desc.size, minimum_size))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  constexpr std::uint64_t default_page_size = UINT64_C(1024) * UINT64_C(1024);
  const auto page_size = std::max(default_page_size, minimum_size);
  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = desc.usage;
  buffer_desc.memory_location = desc.memory_location;
  buffer_desc.size = page_size;
  granit_buffer buffer{};
  const auto create_result = create_buffer(renderer, buffer_desc, buffer);
  if (create_result != GRANIT_SUCCESS)
    return create_result;
  {
    std::lock_guard lock{mutex_};
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    found->second->owned_by_frame_context = true;
  }
  try {
    slot->transient_pages.push_back({.buffer = buffer,
                                     .usage = desc.usage,
                                     .memory_location = desc.memory_location,
                                     .size = page_size,
                                     .offset = desc.size});
  } catch (...) {
    {
      std::lock_guard lock{mutex_};
      const auto found = buffers_.find(buffer);
      if (found != buffers_.end())
        found->second->owned_by_frame_context = false;
    }
    static_cast<void>(destroy_buffer(renderer, buffer));
    throw;
  }
  slice = {.buffer = buffer, .offset = 0, .size = desc.size, .usage = desc.usage, .reserved = 0};
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::submit_frame_context(granit_renderer renderer,
                                                      granit_frame_context context,
                                                      granit_frame frame) {
  std::shared_ptr<frame_context_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = frame_contexts_.find(context);
    const auto renderer_found = backend_renderers_.find(renderer);
    if (found == frame_contexts_.end() || renderer_found == backend_renderers_.end() ||
        found->second->owner != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  const auto slot = std::find_if(record->slots.begin(), record->slots.end(), [&](const auto& item) {
    return item.state == frame_context_slot_state::recording && item.frame == frame;
  });
  if (slot == record->slots.end())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto end_result = end_command_recorder(renderer, slot->recorder);
  if (end_result != GRANIT_SUCCESS)
    return end_result;
  const auto submit_result = submit_command_recorder_frame(renderer, slot->recorder, frame);
  if (submit_result != GRANIT_SUCCESS)
    return submit_result;
  slot->state = frame_context_slot_state::submitted;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::abort_frame_context(granit_renderer renderer,
                                                     granit_frame_context context,
                                                     granit_frame frame) {
  std::shared_ptr<frame_context_record> context_record;
  std::shared_ptr<command_recorder_record> command;
  frame_context_slot* slot{};
  {
    std::lock_guard lock{mutex_};
    const auto found = frame_contexts_.find(context);
    const auto renderer_found = backend_renderers_.find(renderer);
    if (found == frame_contexts_.end() || renderer_found == backend_renderers_.end() ||
        found->second->owner != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    context_record = found->second;
  }
  std::lock_guard context_lock{context_record->mutex};
  const auto found_slot = std::find_if(
      context_record->slots.begin(), context_record->slots.end(), [&](const auto& item) {
        return item.state == frame_context_slot_state::recording && item.frame == frame;
      });
  if (found_slot == context_record->slots.end())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  slot = &*found_slot;
  command = acquire_command_recorder(renderer, slot->recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard command_lock{command->mutex};
  const auto discard_result = command->commands->discard_command_recorder(*command->native);
  if (discard_result != GRANIT_SUCCESS)
    return discard_result;
  command->retained_resources.clear();
  const auto result = command->commands->create_command_recorder(*command->native);
  if (result == GRANIT_SUCCESS) {
    slot->frame = GRANIT_NULL_HANDLE;
    slot->state = frame_context_slot_state::idle;
    for (auto& page : slot->transient_pages)
      page.offset = 0;
  }
  return result;
}

granit_result renderer_registry::destroy_frame_context(granit_renderer renderer,
                                                       granit_frame_context context) {
  std::shared_ptr<frame_context_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = frame_contexts_.find(context);
    const auto renderer_found = backend_renderers_.find(renderer);
    if (found == frame_contexts_.end() || renderer_found == backend_renderers_.end() ||
        found->second->owner != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    frame_contexts_.erase(found);
    static_cast<void>(
        handles_.erase(context, resource_type::frame_context, renderer_found->second->domain()));
    for (const auto& slot : record->slots) {
      const auto command = command_recorders_.find(slot.recorder);
      if (command != command_recorders_.end())
        command->second->owned_by_frame_context = false;
    }
  }
  granit_result result = GRANIT_SUCCESS;
  for (const auto& slot : record->slots) {
    const auto destroy_result = destroy_command_recorder(renderer, slot.recorder);
    if (result == GRANIT_SUCCESS && destroy_result != GRANIT_SUCCESS)
      result = destroy_result;
  }
  for (const auto& slot : record->slots) {
    for (const auto& page : slot.transient_pages) {
      {
        std::lock_guard lock{mutex_};
        const auto found = buffers_.find(page.buffer);
        if (found != buffers_.end())
          found->second->owned_by_frame_context = false;
      }
      const auto destroy_result = destroy_buffer(renderer, page.buffer);
      if (result == GRANIT_SUCCESS && destroy_result != GRANIT_SUCCESS)
        result = destroy_result;
    }
  }
  return result;
}

} // namespace granit::detail
