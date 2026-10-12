// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/resource_table.h>

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include <algorithm>
#include <limits>
#include <new>

namespace granit::detail {
namespace {

constexpr std::uint32_t all_resource_types =
    GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW_BIT |
    GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT |
    GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER_BIT;
constexpr std::uint32_t maximum_capacity = UINT32_C(1) << 20;

bool is_valid_resource_type(std::uint32_t type) noexcept {
  return type == GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW ||
         type == GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER ||
         type == GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER;
}

std::uint32_t resource_type_bit(std::uint32_t type) noexcept {
  switch (type) {
  case GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW:
    return GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW_BIT;
  case GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER:
    return GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT;
  case GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER:
    return GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER_BIT;
  default:
    return 0;
  }
}

} // namespace

granit_result renderer_registry::get_resource_table_capabilities(
    granit_renderer renderer, granit_resource_table_capabilities& capabilities) {
  const auto state = acquire_backend(renderer);
  if (!state) return GRANIT_ERROR_INVALID_HANDLE;
  const bool has_descriptor_indexing =
      (state->capabilities().renderer_features &
       GRANIT_RENDERER_FEATURE_BINDLESS_DESCRIPTOR_INDEXING_BIT) != 0;
  capabilities.support = has_descriptor_indexing ? GRANIT_RESOURCE_TABLE_SUPPORT_LOGICAL
                                                  : GRANIT_RESOURCE_TABLE_SUPPORT_FALLBACK;
  capabilities.resource_types = all_resource_types;
  capabilities.max_sampled_texture_views = maximum_capacity;
  capabilities.max_samplers = maximum_capacity;
  capabilities.max_storage_buffers = maximum_capacity;
  capabilities.reserved[0] = 0;
  capabilities.reserved[1] = 0;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_resource_table(
    granit_renderer renderer, const granit_resource_table_desc& desc,
    granit_resource_table& table) {
  const auto interfaces = acquire_backend_interfaces(renderer);
  if (!interfaces) return GRANIT_ERROR_INVALID_HANDLE;

  granit_resource_table_capabilities capabilities = GRANIT_RESOURCE_TABLE_CAPABILITIES_INIT;
  capabilities.struct_size = GRANIT_RESOURCE_TABLE_CAPABILITIES_VERSION_1_SIZE;
  const auto capability_result = get_resource_table_capabilities(renderer, capabilities);
  if (capability_result != GRANIT_SUCCESS) return capability_result;
  if (desc.binding_mode == GRANIT_RESOURCE_TABLE_BINDING_MODE_EXPLICIT &&
      capabilities.support != GRANIT_RESOURCE_TABLE_SUPPORT_LOGICAL &&
      capabilities.support != GRANIT_RESOURCE_TABLE_SUPPORT_NATIVE) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (desc.resource_types == 0 || (desc.resource_types & ~all_resource_types) != 0 ||
      desc.binding_mode > GRANIT_RESOURCE_TABLE_BINDING_MODE_EXPLICIT ||
      desc.sampled_texture_view_capacity > maximum_capacity ||
      desc.sampler_capacity > maximum_capacity || desc.storage_buffer_capacity > maximum_capacity)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((desc.resource_types & GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW_BIT) != 0 &&
      desc.sampled_texture_view_capacity == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((desc.resource_types & GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT) != 0 &&
      desc.sampler_capacity == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((desc.resource_types & GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER_BIT) != 0 &&
      desc.storage_buffer_capacity == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  auto record = std::make_shared<resource_table_record>();
  record->owner = interfaces->renderer;
  record->binding_mode = desc.binding_mode;
  record->resource_types = desc.resource_types;
  record->sampled_texture_views.resize(desc.sampled_texture_view_capacity);
  record->samplers.resize(desc.sampler_capacity);
  record->storage_buffers.resize(desc.storage_buffer_capacity);

  std::lock_guard lock{mutex_};
  if (backend_renderers_.find(renderer) == backend_renderers_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  record->metadata.creation_sequence = next_creation_sequence_++;
  const auto handle = handles_.insert(record.get(), resource_type::resource_table,
                                      interfaces->renderer->domain());
  if (handle == GRANIT_NULL_HANDLE) return GRANIT_ERROR_OUT_OF_MEMORY;
  try {
    resource_tables_.emplace(handle, std::move(record));
  } catch (...) {
    static_cast<void>(handles_.erase(handle, resource_type::resource_table,
                                     interfaces->renderer->domain()));
    throw;
  }
  table = handle;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::update_resource_table(
    granit_renderer renderer, granit_resource_table table,
    const granit_resource_table_update_desc& update) {
  return update_resource_table_batch(renderer, table, std::span{&update, 1});
}

granit_result renderer_registry::update_resource_table_batch(
    granit_renderer renderer, granit_resource_table table,
    std::span<const granit_resource_table_update_desc> updates) {
  if (updates.empty()) return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard lock{mutex_};
  const auto state = backend_renderers_.find(renderer);
  if (state == backend_renderers_.end() ||
      handles_.find(table, resource_type::resource_table, state->second->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = resource_tables_.find(table);
  if (found == resource_tables_.end() || found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& record = *found->second;

  struct validated_update {
    std::vector<resource_table_slot>* slots;
    std::uint32_t slot;
  };
  std::vector<validated_update> validated;
  validated.reserve(updates.size());
  for (const auto& update : updates) {
    if (!is_valid_resource_type(update.type) ||
        (record.resource_types & resource_type_bit(update.type)) == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    std::vector<resource_table_slot>* slots = nullptr;
    resource_type expected = resource_type::unknown;
    switch (update.type) {
    case GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW:
      slots = &record.sampled_texture_views;
      expected = resource_type::texture_view;
      break;
    case GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER:
      slots = &record.samplers;
      expected = resource_type::sampler;
      break;
    case GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER:
      slots = &record.storage_buffers;
      expected = resource_type::buffer;
      break;
    default:
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    if (update.slot >= slots->size()) return GRANIT_ERROR_INVALID_ARGUMENT;
    if (update.resource == GRANIT_NULL_HANDLE) {
      if (update.offset != 0 || update.size != 0) return GRANIT_ERROR_INVALID_ARGUMENT;
    } else if (handles_.find(update.resource, expected, state->second->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    } else if (expected == resource_type::buffer) {
      const auto buffer = buffers_.find(update.resource);
      if (buffer == buffers_.end() || update.offset > buffer->second->desc.size)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto remaining = buffer->second->desc.size - update.offset;
      if (update.size == 0 || (update.size != GRANIT_WHOLE_SIZE && update.size > remaining))
        return GRANIT_ERROR_INVALID_ARGUMENT;
    } else if (update.offset != 0 || update.size != 0) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    validated.push_back({slots, update.slot});
  }
  for (std::size_t index = 0; index < updates.size(); ++index) {
    const auto& update = updates[index];
    auto& slot = (*validated[index].slots)[validated[index].slot];
    slot.resource = update.resource;
    slot.offset = update.offset;
    slot.size = update.size;
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_resource_table(granit_renderer renderer,
                                                        granit_resource_table table) {
  std::lock_guard lock{mutex_};
  const auto state = backend_renderers_.find(renderer);
  if (state == backend_renderers_.end() ||
      handles_.find(table, resource_type::resource_table, state->second->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = resource_tables_.find(table);
  if (found == resource_tables_.end() || found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  resource_tables_.erase(found);
  return handles_.erase(table, resource_type::resource_table, state->second->domain());
}

} // namespace granit::detail

extern "C" granit_result granit_renderer_get_resource_table_capabilities(
    granit_renderer renderer, granit_resource_table_capabilities* capabilities) {
  if (capabilities == nullptr ||
      capabilities->struct_size < GRANIT_RESOURCE_TABLE_CAPABILITIES_VERSION_1_SIZE ||
      capabilities->reserved[0] != 0 || capabilities->reserved[1] != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE) return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().get_resource_table_capabilities(
        renderer, *capabilities);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_resource_table_create(
    granit_renderer renderer, const granit_resource_table_desc* desc,
    granit_resource_table* table) {
  if (desc == nullptr || table == nullptr ||
      desc->struct_size < GRANIT_RESOURCE_TABLE_DESC_VERSION_1_SIZE ||
      desc->reserved[0] != 0 || desc->reserved[1] != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *table = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE) return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().create_resource_table(renderer, *desc,
                                                                                 *table);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_resource_table_update(
    granit_renderer renderer, granit_resource_table table,
    const granit_resource_table_update_desc* update) {
  if (update == nullptr) return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE || table == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().update_resource_table(renderer, table,
                                                                                  *update);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_resource_table_update_batch(
    granit_renderer renderer, granit_resource_table table,
    const granit_resource_table_update_desc* updates, uint32_t update_count) {
  if (updates == nullptr || update_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE || table == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().update_resource_table_batch(
        renderer, table, std::span{updates, update_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_resource_table_destroy(granit_renderer renderer,
                                                        granit_resource_table table) {
  if (renderer == GRANIT_NULL_HANDLE || table == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_resource_table(renderer, table);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
