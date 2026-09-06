// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "assets/shader_asset.h"
#include "core/async_operation_state.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <type_traits>

namespace granit::detail {
namespace {

template <typename Entry> void refresh_graphics_pointers(Entry& entry) {
  auto& desc = entry.graphics;
  desc.color_formats = entry.color_formats.empty() ? nullptr : entry.color_formats.data();
  desc.vertex_buffer_layouts = entry.vertex_buffers.empty() ? nullptr : entry.vertex_buffers.data();
  for (std::size_t index = 0; index < entry.vertex_buffers.size(); ++index) {
    entry.vertex_buffers[index].attributes = entry.vertex_attributes[index].empty()
                                                  ? nullptr
                                                  : entry.vertex_attributes[index].data();
  }
  desc.depth = desc.depth == nullptr ? nullptr : &entry.depth;
  desc.depth_bias = desc.depth_bias == nullptr ? nullptr : &entry.depth_bias;
  desc.color_blends = entry.color_blends.empty() ? nullptr : entry.color_blends.data();
}

template <typename T> void append_value(std::vector<std::byte>& bytes, T value) {
  static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
  const auto normalized = static_cast<std::uint64_t>(value);
  for (std::size_t index = 0; index < sizeof(T); ++index)
    bytes.push_back(static_cast<std::byte>((normalized >> (index * 8U)) & 0xffU));
}

template <typename T> void append_structure(std::vector<std::byte>& bytes, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  const auto span = std::as_bytes(std::span{&value, std::size_t{1}});
  bytes.insert(bytes.end(), span.begin(), span.end());
}

} // namespace

granit_result renderer_registry::create_pipeline_warmup_batch(
    granit_renderer renderer, const granit_pipeline_warmup_batch_desc& desc,
    granit_pipeline_warmup_batch& batch) {
  try {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<pipeline_warmup_batch_record>();
    record->owner = owner->second;
    record->max_operation_count = desc.max_operation_count;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::pipeline_warmup_batch,
                                        owner->second->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    pipeline_warmup_batches_.emplace(handle, std::move(record));
    batch = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::add_graphics_pipeline_warmup(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    const granit_graphics_pipeline_desc& desc, std::uint32_t& result_index) {
  if (desc.struct_size < GRANIT_GRAPHICS_PIPELINE_DESC_SIZE ||
      (desc.color_format_count != 0 && desc.color_formats == nullptr) ||
      (desc.vertex_buffer_layout_count != 0 && desc.vertex_buffer_layouts == nullptr) ||
      (desc.color_blend_count != 0 && desc.color_blends == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::shared_ptr<pipeline_warmup_batch_record> record;
    {
      std::lock_guard lock{mutex_};
      const auto owner = backend_renderers_.find(renderer);
      const auto found = pipeline_warmup_batches_.find(batch);
      if (owner == backend_renderers_.end() || found == pipeline_warmup_batches_.end() ||
          found->second->owner != owner->second)
        return GRANIT_ERROR_INVALID_HANDLE;
      record = found->second;
    }
    pipeline_warmup_entry entry;
    entry.type = GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS;
    entry.graphics = desc;
    if (desc.color_format_count != 0)
      entry.color_formats.assign(desc.color_formats, desc.color_formats + desc.color_format_count);
    if (desc.color_blend_count != 0)
      entry.color_blends.assign(desc.color_blends, desc.color_blends + desc.color_blend_count);
    else {
      entry.color_blends.resize(desc.color_format_count, GRANIT_COLOR_BLEND_STATE_INIT);
      entry.graphics.color_blend_count = desc.color_format_count;
    }
    if (desc.vertex_buffer_layout_count != 0)
      entry.vertex_buffers.assign(desc.vertex_buffer_layouts,
                                  desc.vertex_buffer_layouts + desc.vertex_buffer_layout_count);
    entry.vertex_attributes.resize(desc.vertex_buffer_layout_count);
    for (std::size_t index = 0; index < entry.vertex_buffers.size(); ++index) {
      const auto& layout = desc.vertex_buffer_layouts[index];
      if (layout.attribute_count != 0 && layout.attributes == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      if (layout.attribute_count != 0)
        entry.vertex_attributes[index].assign(layout.attributes,
                                              layout.attributes + layout.attribute_count);
    }
    entry.depth = {desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED,
                   desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED,
                   GRANIT_COMPARE_OPERATION_LESS_EQUAL, 0};
    if (desc.depth)
      entry.depth = *desc.depth;
    if (desc.depth_bias)
      entry.depth_bias = *desc.depth_bias;
    refresh_graphics_pointers(entry);
    std::lock_guard lock{record->mutex};
    if (record->max_operation_count != 0 &&
        record->entries.size() >= record->max_operation_count)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    result_index = static_cast<std::uint32_t>(record->entries.size());
    record->entries.push_back(std::move(entry));
    refresh_graphics_pointers(record->entries.back());
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::add_compute_pipeline_warmup(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    const granit_compute_pipeline_desc& desc, std::uint32_t& result_index) {
  if (desc.struct_size < GRANIT_COMPUTE_PIPELINE_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::shared_ptr<pipeline_warmup_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = pipeline_warmup_batches_.find(batch);
    if (owner == backend_renderers_.end() || found == pipeline_warmup_batches_.end() ||
        found->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  try {
    std::lock_guard lock{record->mutex};
    if (record->max_operation_count != 0 &&
        record->entries.size() >= record->max_operation_count)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    pipeline_warmup_entry entry;
    entry.type = GRANIT_PIPELINE_WARMUP_TYPE_COMPUTE;
    entry.compute = desc;
    result_index = static_cast<std::uint32_t>(record->entries.size());
    record->entries.push_back(std::move(entry));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::get_pipeline_warmup_batch_info(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    granit_pipeline_warmup_batch_info& info) {
  std::shared_ptr<pipeline_warmup_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = pipeline_warmup_batches_.find(batch);
    if (owner == backend_renderers_.end() || found == pipeline_warmup_batches_.end() ||
        found->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  info.operation_count = static_cast<std::uint32_t>(record->entries.size());
  info.max_operation_count = record->max_operation_count;
  info.reserved = 0;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::make_pipeline_warmup_key(
    granit_renderer renderer, const pipeline_warmup_entry& entry,
    std::array<std::uint8_t, GRANIT_PIPELINE_WARMUP_CACHE_KEY_SIZE>& key) {
  std::vector<std::byte> bytes;
  try {
    append_value(bytes, UINT32_C(1));
    append_value(bytes, entry.type);
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    if (owner == backend_renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& capabilities = owner->second->capabilities();
    append_value(bytes, owner->second->backend());
    append_value(bytes, capabilities.uniform_buffer_offset_alignment);
    append_value(bytes, capabilities.storage_buffer_offset_alignment);
    append_value(bytes, capabilities.max_uniform_buffer_binding_size);
    append_value(bytes, capabilities.max_storage_buffer_binding_size);
    append_value(bytes, capabilities.framebuffer_sample_counts);
    append_value(bytes, capabilities.renderer_features);
    append_value(bytes, capabilities.shader_features);
    append_value(bytes, capabilities.shader_profile);
    const auto& layout_handle = entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS
                                    ? entry.graphics.layout
                                    : entry.compute.layout;
    const auto layout = entry.retained_layout ? entry.retained_layout
                                              : (pipeline_layouts_.contains(layout_handle)
                                                     ? pipeline_layouts_.at(layout_handle)
                                                     : nullptr);
    if (!layout || layout->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    for (const auto& group : layout->bind_group_layouts) {
      append_value(bytes, static_cast<std::uint32_t>(group->entries.size()));
      for (const auto& binding : group->entries)
        append_structure(bytes, binding);
    }
    if (entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS) {
      const auto vertex = entry.retained_vertex_shader;
      const auto fragment = entry.retained_fragment_shader;
      if (!vertex || !fragment || vertex->owner != owner->second ||
          fragment->owner != owner->second)
        return GRANIT_ERROR_INVALID_HANDLE;
      bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(vertex->content_id.data()),
                   reinterpret_cast<const std::byte*>(vertex->content_id.data() +
                                                      vertex->content_id.size()));
      bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(fragment->content_id.data()),
                   reinterpret_cast<const std::byte*>(fragment->content_id.data() +
                                                      fragment->content_id.size()));
      append_value(bytes, static_cast<std::uint32_t>(vertex->entry_point.size()));
      bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(vertex->entry_point.data()),
                   reinterpret_cast<const std::byte*>(vertex->entry_point.data() +
                                                      vertex->entry_point.size()));
      append_value(bytes, static_cast<std::uint32_t>(fragment->entry_point.size()));
      bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(fragment->entry_point.data()),
                   reinterpret_cast<const std::byte*>(fragment->entry_point.data() +
                                                      fragment->entry_point.size()));
      append_value(bytes, static_cast<std::uint32_t>(entry.color_formats.size()));
      for (const auto format : entry.color_formats)
        append_value(bytes, format);
      append_value(bytes, entry.graphics.depth_stencil_format);
      append_value(bytes, entry.graphics.sample_count);
      append_structure(bytes, entry.graphics.primitive);
      append_value(bytes, entry.graphics.depth != nullptr ? UINT32_C(1) : UINT32_C(0));
      if (entry.graphics.depth)
        append_structure(bytes, entry.depth);
      if (entry.graphics.depth_bias)
        append_structure(bytes, entry.depth_bias);
      append_value(bytes, static_cast<std::uint32_t>(entry.vertex_buffers.size()));
      for (std::size_t index = 0; index < entry.vertex_buffers.size(); ++index) {
        append_value(bytes, entry.vertex_buffers[index].stride);
        append_value(bytes, entry.vertex_buffers[index].step_mode);
        append_value(bytes, static_cast<std::uint32_t>(entry.vertex_attributes[index].size()));
        for (const auto& attribute : entry.vertex_attributes[index])
          append_structure(bytes, attribute);
      }
      for (const auto& blend : entry.color_blends)
        append_structure(bytes, blend);
    } else {
      const auto compute = entry.retained_compute_shader;
      if (!compute || compute->owner != owner->second)
        return GRANIT_ERROR_INVALID_HANDLE;
      bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(compute->content_id.data()),
                   reinterpret_cast<const std::byte*>(compute->content_id.data() +
                                                      compute->content_id.size()));
      append_value(bytes, static_cast<std::uint32_t>(compute->entry_point.size()));
      bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(compute->entry_point.data()),
                   reinterpret_cast<const std::byte*>(compute->entry_point.data() +
                                                      compute->entry_point.size()));
    }
    const auto digest = granit::tools::shader_bytes_sha256(bytes);
    for (std::size_t index = 0; index < digest.size(); ++index)
      key[index] = static_cast<std::uint8_t>(digest[index]);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::submit_pipeline_warmup_batch_async(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    granit_async_operation& operation) {
  try {
    std::shared_ptr<pipeline_warmup_batch_record> record;
    {
      std::lock_guard lock{mutex_};
      const auto owner = backend_renderers_.find(renderer);
      const auto found = pipeline_warmup_batches_.find(batch);
      if (owner == backend_renderers_.end() || found == pipeline_warmup_batches_.end() ||
          found->second->owner != owner->second)
        return GRANIT_ERROR_INVALID_HANDLE;
      record = found->second;
    }
    auto payload = std::make_shared<pipeline_warmup_batch_operation>();
    payload->renderer = renderer;
    {
      std::lock_guard lock{record->mutex};
      if (record->entries.empty())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      payload->entries = record->entries;
      for (auto& entry : payload->entries) {
        if (entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS)
          refresh_graphics_pointers(entry);
        entry.result = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
        entry.result.type = entry.type;
      }
    }
    {
      std::lock_guard lock{mutex_};
      for (auto& entry : payload->entries) {
        const auto layout_handle = entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS
                                       ? entry.graphics.layout
                                       : entry.compute.layout;
        const auto layout = pipeline_layouts_.find(layout_handle);
        if (layout == pipeline_layouts_.end() || layout->second->owner != record->owner)
          return GRANIT_ERROR_INVALID_HANDLE;
        entry.retained_layout = layout->second;
        if (entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS) {
          const auto vertex = shaders_.find(entry.graphics.vertex_shader);
          const auto fragment = shaders_.find(entry.graphics.fragment_shader);
          if (vertex == shaders_.end() || fragment == shaders_.end() ||
              vertex->second->owner != record->owner || fragment->second->owner != record->owner)
            return GRANIT_ERROR_INVALID_HANDLE;
          entry.retained_vertex_shader = vertex->second;
          entry.retained_fragment_shader = fragment->second;
        } else {
          const auto compute = shaders_.find(entry.compute.compute_shader);
          if (compute == shaders_.end() || compute->second->owner != record->owner)
            return GRANIT_ERROR_INVALID_HANDLE;
          entry.retained_compute_shader = compute->second;
        }
      }
    }
    auto state = std::make_shared<async_operation_state_machine>();
    if (!state->begin())
      return GRANIT_ERROR_INTERNAL;
    const auto poll = [this, payload, state]() {
      std::lock_guard payload_lock{payload->mutex};
      const auto status = state->status();
      if (status.state != GRANIT_ASYNC_OPERATION_STATE_RUNNING)
        return;
      if (payload->pending) {
        const auto pending_result = payload->pending->poll();
        if (pending_result == GRANIT_ERROR_NOT_READY)
          return;
        auto& entry = payload->entries[payload->next_index];
        entry.result.result = pending_result;
        if (entry.result.result == GRANIT_SUCCESS) {
          std::lock_guard lock{mutex_};
          warmed_pipeline_keys_[std::move(payload->pending_cache_key)] = payload->pending_owner;
        }
        payload->pending.reset();
        payload->pending_owner.reset();
        ++payload->next_index;
        if (payload->next_index == payload->entries.size())
          state->complete(GRANIT_SUCCESS);
        return;
      }
      if (status.cancel_requested != 0) {
        for (; payload->next_index < payload->entries.size(); ++payload->next_index)
          payload->entries[payload->next_index].result.result = GRANIT_ERROR_CANCELLED;
        state->acknowledge_cancel();
        return;
      }
      auto& entry = payload->entries[payload->next_index];
      std::array<std::uint8_t, GRANIT_PIPELINE_WARMUP_CACHE_KEY_SIZE> key{};
      auto result = make_pipeline_warmup_key(payload->renderer, entry, key);
      std::copy(key.begin(), key.end(), entry.result.cache_key);
      std::string cache_key(reinterpret_cast<const char*>(key.data()), key.size());
      std::shared_ptr<backend_renderer> owner;
      if (result == GRANIT_SUCCESS) {
        std::lock_guard lock{mutex_};
        owner = backend_renderers_.contains(payload->renderer)
                    ? backend_renderers_.at(payload->renderer)
                    : nullptr;
        cache_key.append(reinterpret_cast<const char*>(&payload->renderer),
                         sizeof(payload->renderer));
        const auto found = warmed_pipeline_keys_.find(cache_key);
        if (found != warmed_pipeline_keys_.end() && found->second.lock() == owner)
          entry.result.cache_hit = 1;
      }
      if (result == GRANIT_SUCCESS && entry.result.cache_hit == 0) {
        std::shared_ptr<backend_pipeline_warmup_renderer> warmup;
        {
          std::lock_guard lock{mutex_};
          const auto found = backend_interfaces_.find(payload->renderer);
          if (found != backend_interfaces_.end())
            warmup = found->second->pipeline_warmup;
        }
        if (warmup) {
          payload->pending_cache_key = std::move(cache_key);
          payload->pending_owner = owner;
          if (!entry.retained_layout ||
              (entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS &&
               (!entry.retained_vertex_shader || !entry.retained_fragment_shader)) ||
              (entry.type == GRANIT_PIPELINE_WARMUP_TYPE_COMPUTE &&
               !entry.retained_compute_shader)) {
            result = GRANIT_ERROR_INVALID_HANDLE;
          } else if (entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS) {
            const backend_graphics_pipeline_create_info info{
                *entry.retained_layout->native,
                *entry.retained_vertex_shader->native,
                entry.retained_vertex_shader->entry_point.c_str(),
                *entry.retained_fragment_shader->native,
                entry.retained_fragment_shader->entry_point.c_str(),
                {entry.graphics.vertex_buffer_layouts,
                 entry.graphics.vertex_buffer_layout_count},
                entry.graphics.primitive,
                entry.depth,
                entry.graphics.depth_bias,
                {entry.graphics.color_blends, entry.graphics.color_blend_count},
                {entry.graphics.color_formats, entry.graphics.color_format_count},
                entry.graphics.depth_stencil_format,
                entry.graphics.sample_count};
            result = warmup->warmup_graphics_pipeline_async(info, payload->pending);
          } else {
            result = warmup->warmup_compute_pipeline_async(
                *entry.retained_layout->native, *entry.retained_compute_shader->native,
                entry.retained_compute_shader->entry_point.c_str(), payload->pending);
          }
          if (result == GRANIT_SUCCESS && payload->pending)
            return;
          payload->pending.reset();
          payload->pending_owner.reset();
          payload->pending_cache_key.clear();
        }
        if (!warmup && entry.type == GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS) {
          granit_graphics_pipeline pipeline{};
          result = create_graphics_pipeline(payload->renderer, entry.graphics, pipeline);
          if (result == GRANIT_SUCCESS)
            result = destroy_graphics_pipeline(payload->renderer, pipeline);
        } else if (!warmup) {
          granit_compute_pipeline pipeline{};
          result = create_compute_pipeline(payload->renderer, entry.compute, pipeline);
          if (result == GRANIT_SUCCESS)
            result = destroy_compute_pipeline(payload->renderer, pipeline);
        }
        if (result == GRANIT_SUCCESS) {
          std::lock_guard lock{mutex_};
          warmed_pipeline_keys_[std::move(cache_key)] = owner;
        }
      }
      entry.result.result = result;
      ++payload->next_index;
      if (payload->next_index == payload->entries.size())
        state->complete(GRANIT_SUCCESS);
    };
    return register_async_operation(renderer, std::move(state), operation, poll,
                                    std::move(payload), async_operation_kind::pipeline_warmup_batch);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_pipeline_warmup_result(
    granit_renderer renderer, granit_async_operation operation, std::uint32_t result_index,
    granit_pipeline_warmup_result_info& info) {
  std::shared_ptr<async_operation_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = async_operations_.find(operation);
    if (owner == backend_renderers_.end() || found == async_operations_.end() ||
        found->second->owner != owner->second ||
        found->second->kind != async_operation_kind::pipeline_warmup_batch)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  if (record->poll)
    record->poll();
  const auto payload = std::static_pointer_cast<pipeline_warmup_batch_operation>(record->payload);
  std::lock_guard lock{payload->mutex};
  if (result_index >= payload->entries.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (result_index >= payload->next_index)
    return GRANIT_ERROR_NOT_READY;
  const auto size = info.struct_size;
  info = payload->entries[result_index].result;
  info.struct_size = size;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::reset_pipeline_warmup_batch(
    granit_renderer renderer, granit_pipeline_warmup_batch batch) {
  std::shared_ptr<pipeline_warmup_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto owner = backend_renderers_.find(renderer);
    const auto found = pipeline_warmup_batches_.find(batch);
    if (owner == backend_renderers_.end() || found == pipeline_warmup_batches_.end() ||
        found->second->owner != owner->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  record->entries.clear();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_pipeline_warmup_batch(
    granit_renderer renderer, granit_pipeline_warmup_batch batch) {
  std::lock_guard lock{mutex_};
  const auto owner = backend_renderers_.find(renderer);
  const auto found = pipeline_warmup_batches_.find(batch);
  if (owner == backend_renderers_.end() || found == pipeline_warmup_batches_.end() ||
      found->second->owner != owner->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto result = handles_.erase(batch, resource_type::pipeline_warmup_batch,
                                     owner->second->domain());
  if (result == GRANIT_SUCCESS)
    pipeline_warmup_batches_.erase(found);
  return result;
}

} // namespace granit::detail
