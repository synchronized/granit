// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "renderer/renderer_state.h"
#include "renderer/shader_validation.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result renderer_registry::create_shader_from_desc(granit_renderer renderer,
                                                         const granit_shader_desc& desc,
                                                         granit_shader& shader) {
  const auto owner = acquire_backend(renderer);
  if (!owner) {
    const auto validation = desc.struct_size >= GRANIT_SHADER_DESC_VERSION_2_SIZE &&
                                    (desc.wgsl != nullptr || desc.wgsl_length != 0)
                                ? validate_shader_wgsl(&desc)
                                : validate_shader_spirv(&desc);
    return validation == GRANIT_SUCCESS ? GRANIT_ERROR_INVALID_HANDLE : validation;
  }
  if (std::dynamic_pointer_cast<backend_shader_renderer>(owner)) {
    const auto validation = validate_shader_wgsl(&desc);
    if (validation != GRANIT_SUCCESS)
      return validation;
    return create_wgsl_shader(
        renderer, desc.stage, {desc.wgsl, static_cast<std::size_t>(desc.wgsl_length)},
        {desc.entry_point, static_cast<std::size_t>(desc.entry_point_length)}, shader);
  }
  const auto validation = validate_shader_spirv(&desc);
  if (validation != GRANIT_SUCCESS)
    return validation;
  std::vector<std::uint32_t> code(static_cast<std::size_t>(desc.code_size) / sizeof(std::uint32_t));
  std::memcpy(code.data(), desc.code, static_cast<std::size_t>(desc.code_size));
  return create_shader(renderer, desc.stage, code,
                       std::string_view{desc.entry_point, desc.entry_point_length}, shader);
}

granit_result renderer_registry::create_shader(granit_renderer renderer, granit_shader_stage stage,
                                               std::span<const std::uint32_t> code,
                                               std::string_view entry_point,
                                               granit_shader& shader) {
  try {
    auto state = std::dynamic_pointer_cast<renderer_state>(acquire_backend(renderer));
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<shader_record>();
    record->owner = state;
    record->stage = stage;
    record->entry_point.assign(entry_point);
    record->native = state->allocate_shader_resource();
    const auto result = state->create_native_shader(code, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::shader, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      shaders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::shader, state->domain()));
      throw;
    }
    shader = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::create_wgsl_shader(granit_renderer renderer,
                                                    granit_shader_stage stage,
                                                    std::string_view source,
                                                    std::string_view entry_point,
                                                    granit_shader& shader) {
  try {
    auto owner = acquire_backend(renderer);
    auto shaders = std::dynamic_pointer_cast<backend_shader_renderer>(owner);
    if (!owner || !shaders)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<shader_record>();
    record->owner = owner;
    record->stage = stage;
    record->entry_point.assign(entry_point);
    record->native = shaders->allocate_shader_resource();
    if (!record->native)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto result = shaders->create_wgsl_shader(*record->native, stage, source, entry_point);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::shader, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      shaders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::shader, owner->domain()));
      throw;
    }
    shader = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_shader(granit_renderer renderer, granit_shader shader) {
  std::shared_ptr<shader_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = found_renderer->second;
    if (handles_.find(shader, resource_type::shader, owner->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = shaders_.find(shader);
    if (found == shaders_.end() || found->second->owner != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    shaders_.erase(found);
    static_cast<void>(handles_.erase(shader, resource_type::shader, owner->domain()));
  }
  auto state = std::dynamic_pointer_cast<renderer_state>(record->owner);
  if (state) {
    state->retire_resource(record->metadata.last_use_serial.load(), retirement_order::resource,
                           record);
  }
  record.reset();
  if (state)
    static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result
renderer_registry::create_bind_group_layout(granit_renderer renderer,
                                            std::span<const granit_bind_group_layout_entry> entries,
                                            granit_bind_group_layout& layout) {
  try {
    auto state = std::dynamic_pointer_cast<renderer_state>(acquire_backend(renderer));
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<bind_group_layout_record>();
    record->owner = state;
    record->resource_api = state;
    record->retirement = state;
    record->entries.assign(entries.begin(), entries.end());
    record->native = record->resource_api->allocate_bind_group_layout_resource();
    const auto result = record->resource_api->create_bind_group_layout(entries, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (backend_renderers_.find(renderer) == backend_renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::bind_group_layout, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      bind_group_layouts_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::bind_group_layout, state->domain()));
      throw;
    }
    layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_bind_group_layout(granit_renderer renderer,
                                                           granit_bind_group_layout layout) {
  std::shared_ptr<bind_group_layout_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = backend_renderers_.find(renderer);
    if (state == backend_renderers_.end() ||
        handles_.find(layout, resource_type::bind_group_layout, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = bind_group_layouts_.find(layout);
    if (found == bind_group_layouts_.end() || found->second->owner != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    bind_group_layouts_.erase(found);
    static_cast<void>(
        handles_.erase(layout, resource_type::bind_group_layout, state->second->domain()));
  }
  const auto retirement = record->retirement;
  const auto serial = record->metadata.last_use_serial.load();
  retirement->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(retirement->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_bind_group(granit_renderer renderer,
                                                   const granit_bind_group_desc& desc,
                                                   granit_bind_group& bind_group) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<bind_group_layout_record> layout;
    auto record = std::make_shared<bind_group_record>();
    std::vector<backend_bind_group_write> writes;
    {
      std::lock_guard lock{mutex_};
      const auto renderer_found = backend_renderers_.find(renderer);
      const auto layout_found = bind_group_layouts_.find(desc.layout);
      if (renderer_found == backend_renderers_.end() || layout_found == bind_group_layouts_.end() ||
          layout_found->second->owner != renderer_found->second)
        return GRANIT_ERROR_INVALID_HANDLE;
      state = std::dynamic_pointer_cast<renderer_state>(renderer_found->second);
      if (!state)
        return GRANIT_ERROR_UNSUPPORTED;
      layout = layout_found->second;
      std::uint64_t required_count{};
      for (const auto& declaration : layout->entries)
        required_count += declaration.array_count;
      if (required_count != desc.entry_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      writes.reserve(desc.entry_count);
      record->resources.reserve(desc.entry_count);
      for (std::uint32_t index = 0; index < desc.entry_count; ++index) {
        const auto& entry = desc.entries[index];
        const auto declaration =
            std::find_if(layout->entries.begin(), layout->entries.end(),
                         [&](const auto& value) { return value.binding == entry.binding; });
        if (declaration == layout->entries.end() || entry.array_element >= declaration->array_count)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        backend_bind_group_write write{.binding = entry.binding,
                                       .array_element = entry.array_element};
        if (declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER ||
            declaration->type == GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER ||
            declaration->type == GRANIT_BINDING_TYPE_STORAGE_BUFFER) {
          const bool uniform = declaration->type != GRANIT_BINDING_TYPE_STORAGE_BUFFER;
          const auto found = buffers_.find(entry.resource);
          if (found == buffers_.end() || found->second->owner != state ||
              entry.offset >= found->second->desc.size || entry.size == 0 ||
              (entry.size != GRANIT_WHOLE_SIZE &&
               entry.size > found->second->desc.size - entry.offset))
            return GRANIT_ERROR_INVALID_ARGUMENT;
          const auto required_usage =
              uniform ? GRANIT_BUFFER_USAGE_UNIFORM_BIT : GRANIT_BUFFER_USAGE_STORAGE_BIT;
          if ((found->second->desc.usage & required_usage) == 0)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          const auto range = entry.size == GRANIT_WHOLE_SIZE
                                 ? found->second->desc.size - entry.offset
                                 : entry.size;
          const auto binding_type =
              uniform ? backend_buffer_binding_type::uniform : backend_buffer_binding_type::storage;
          if (!state->capabilities().supports_buffer_binding(binding_type, entry.offset, range))
            return GRANIT_ERROR_INVALID_ARGUMENT;
          write.type = declaration->type == GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER
                           ? backend_binding_type::dynamic_uniform_buffer
                       : uniform ? backend_binding_type::uniform_buffer
                                 : backend_binding_type::storage_buffer;
          write.buffer = found->second->native.get();
          write.offset = entry.offset;
          write.range = range;
          record->resources.push_back(found->second);
          if (declaration->type == GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER) {
            record->dynamic_uniform_bindings.push_back({
                .binding = entry.binding,
                .base_offset = entry.offset,
                .range = range,
                .buffer_size = found->second->desc.size,
            });
          }
          if ((declaration->visibility &
               (GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT)) != 0) {
            record->graphics_buffer_accesses.push_back({
                .buffer = found->second->native.get(),
                .type = uniform ? backend_buffer_access_type::uniform_read
                                : backend_buffer_access_type::storage_read_write,
            });
          }
          if ((declaration->visibility & GRANIT_SHADER_STAGE_COMPUTE_BIT) != 0) {
            record->compute_buffer_accesses.push_back({
                .buffer = found->second->native.get(),
                .type = uniform ? backend_buffer_access_type::uniform_read
                                : backend_buffer_access_type::storage_read_write,
            });
          }
        } else if (declaration->type == GRANIT_BINDING_TYPE_SAMPLER) {
          const auto found = samplers_.find(entry.resource);
          if (found == samplers_.end() || found->second->owner != state || entry.offset != 0 ||
              entry.size != GRANIT_WHOLE_SIZE)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          write.type = backend_binding_type::sampler;
          write.sampler = found->second->native.get();
          record->resources.push_back(found->second);
        } else {
          const auto found = texture_views_.find(entry.resource);
          if (found == texture_views_.end() || found->second->owner != state || entry.offset != 0 ||
              entry.size != GRANIT_WHOLE_SIZE)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          const auto required_usage = declaration->type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE
                                          ? GRANIT_TEXTURE_USAGE_SAMPLED_BIT
                                          : GRANIT_TEXTURE_USAGE_STORAGE_BIT;
          if ((found->second->texture->desc.usage & required_usage) == 0)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          write.type = declaration->type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE
                           ? backend_binding_type::sampled_texture
                           : backend_binding_type::storage_texture;
          write.texture_view = found->second->native.get();
          record->resources.push_back(found->second);
          const auto make_access = [&] {
            const bool storage = declaration->type == GRANIT_BINDING_TYPE_STORAGE_TEXTURE;
            return backend_texture_access{
                .texture = found->second->texture->native.get(),
                .range = found->second->desc.range,
                .format = found->second->texture->desc.format,
                .type = storage ? backend_texture_access_type::storage_read_write
                                : backend_texture_access_type::sampled_read,
            };
          };
          if ((declaration->visibility &
               (GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT)) != 0) {
            record->graphics_texture_accesses.push_back(make_access());
          }
          if ((declaration->visibility & GRANIT_SHADER_STAGE_COMPUTE_BIT) != 0) {
            record->compute_texture_accesses.push_back(make_access());
          }
        }
        writes.push_back(write);
      }
      sort_dynamic_uniform_bindings(record->dynamic_uniform_bindings);
    }
    record->owner = state;
    record->resource_api = state;
    record->retirement = state;
    record->layout = layout;
    record->native = record->resource_api->allocate_bind_group_resource();
    const auto result =
        record->resource_api->create_bind_group(*layout->native, writes, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (bind_group_layouts_.find(desc.layout) == bind_group_layouts_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::bind_group, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      bind_groups_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::bind_group, state->domain()));
      throw;
    }
    bind_group = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_bind_group(granit_renderer renderer,
                                                    granit_bind_group bind_group) {
  std::shared_ptr<bind_group_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = backend_renderers_.find(renderer);
    if (state == backend_renderers_.end() ||
        handles_.find(bind_group, resource_type::bind_group, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = bind_groups_.find(bind_group);
    if (found == bind_groups_.end() || found->second->owner != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    bind_groups_.erase(found);
    static_cast<void>(
        handles_.erase(bind_group, resource_type::bind_group, state->second->domain()));
  }
  const auto retirement = record->retirement;
  const auto serial = record->metadata.last_use_serial.load();
  retirement->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(retirement->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_pipeline_layout(
    granit_renderer renderer, std::span<const granit_bind_group_layout> bind_group_layouts,
    granit_pipeline_layout& layout) {
  try {
    auto state = std::dynamic_pointer_cast<renderer_state>(acquire_backend(renderer));
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<pipeline_layout_record>();
    record->owner = state;
    std::vector<backend_bind_group_layout_resource*> native_layouts;
    {
      std::lock_guard lock{mutex_};
      for (const auto handle : bind_group_layouts) {
        const auto found = bind_group_layouts_.find(handle);
        if (found == bind_group_layouts_.end() || found->second->owner != state)
          return GRANIT_ERROR_INVALID_HANDLE;
        record->bind_group_layouts.push_back(found->second);
        native_layouts.push_back(found->second->native.get());
      }
    }
    record->native = state->allocate_pipeline_layout_resource();
    const auto result = state->create_native_pipeline_layout(native_layouts, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (backend_renderers_.find(renderer) == backend_renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::pipeline_layout, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      pipeline_layouts_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::pipeline_layout, state->domain()));
      throw;
    }
    layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::create_webgpu_pipeline_layout(granit_renderer renderer,
                                                               granit_pipeline_layout& layout) {
  try {
    auto owner = acquire_backend(renderer);
    auto pipelines = std::dynamic_pointer_cast<backend_pipeline_renderer>(owner);
    if (!owner || !pipelines)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<pipeline_layout_record>();
    record->owner = owner;
    record->native = pipelines->allocate_pipeline_layout_resource();
    if (!record->native)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto result = pipelines->create_empty_pipeline_layout(*record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::pipeline_layout, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    pipeline_layouts_.emplace(handle, std::move(record));
    layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_pipeline_layout(granit_renderer renderer,
                                                         granit_pipeline_layout layout) {
  std::shared_ptr<pipeline_layout_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = backend_renderers_.find(renderer);
    if (state == backend_renderers_.end() ||
        handles_.find(layout, resource_type::pipeline_layout, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = pipeline_layouts_.find(layout);
    if (found == pipeline_layouts_.end() || found->second->owner != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    pipeline_layouts_.erase(found);
    static_cast<void>(
        handles_.erase(layout, resource_type::pipeline_layout, state->second->domain()));
  }
  const auto state = std::dynamic_pointer_cast<renderer_state>(record->owner);
  const auto serial = record->metadata.last_use_serial.load();
  if (state) {
    state->retire_resource(serial, retirement_order::dependent, std::move(record));
    static_cast<void>(state->collect_retired());
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_graphics_pipeline(granit_renderer renderer,
                                                          const granit_graphics_pipeline_desc& desc,
                                                          granit_graphics_pipeline& pipeline) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> vertex;
    std::shared_ptr<shader_record> fragment;
    {
      std::lock_guard lock{mutex_};
      const auto found = backend_renderers_.find(renderer);
      if (found == backend_renderers_.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      state = std::dynamic_pointer_cast<renderer_state>(found->second);
      if (!state)
        return GRANIT_ERROR_UNSUPPORTED;
      const auto layout_found = pipeline_layouts_.find(desc.layout);
      const auto vertex_found = shaders_.find(desc.vertex_shader);
      const auto fragment_found = shaders_.find(desc.fragment_shader);
      if (layout_found == pipeline_layouts_.end() || vertex_found == shaders_.end() ||
          fragment_found == shaders_.end() || layout_found->second->owner != state ||
          vertex_found->second->owner != state || fragment_found->second->owner != state ||
          vertex_found->second->stage != GRANIT_SHADER_STAGE_VERTEX ||
          fragment_found->second->stage != GRANIT_SHADER_STAGE_FRAGMENT)
        return GRANIT_ERROR_INVALID_HANDLE;
      layout = layout_found->second;
      vertex = vertex_found->second;
      fragment = fragment_found->second;
    }
    auto record = std::make_shared<graphics_pipeline_record>();
    record->owner = state;
    record->layout = layout;
    record->vertex_shader = vertex;
    record->fragment_shader = fragment;
    record->native = state->allocate_graphics_pipeline_resource();
    const auto vertex_buffers =
        desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE
            ? std::span<const granit_vertex_buffer_layout>{desc.vertex_buffer_layouts,
                                                           desc.vertex_buffer_layout_count}
            : std::span<const granit_vertex_buffer_layout>{};
    granit_depth_state depth{desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED,
                             desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED,
                             GRANIT_COMPARE_OPERATION_LESS_EQUAL, 0};
    std::array<granit_color_blend_state, 8> default_blends{};
    for (std::size_t index = 0; index < desc.color_format_count; ++index)
      default_blends[index] = {0,
                               GRANIT_BLEND_FACTOR_ONE,
                               GRANIT_BLEND_FACTOR_ZERO,
                               GRANIT_BLEND_OPERATION_ADD,
                               GRANIT_BLEND_FACTOR_ONE,
                               GRANIT_BLEND_FACTOR_ZERO,
                               GRANIT_BLEND_OPERATION_ADD,
                               GRANIT_COLOR_WRITE_ALL_BITS};
    std::span<const granit_color_blend_state> color_blends{default_blends.data(),
                                                           desc.color_format_count};
    if (desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE) {
      if (desc.depth)
        depth = *desc.depth;
      if (desc.color_blend_count != 0)
        color_blends = {desc.color_blends, desc.color_blend_count};
    }
    const auto result = state->create_native_graphics_pipeline(
        *layout->native, *vertex->native, vertex->entry_point.c_str(), *fragment->native,
        fragment->entry_point.c_str(), vertex_buffers,
        desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_3_SIZE
            ? desc.primitive
            : granit_primitive_state{GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                     GRANIT_FRONT_FACE_COUNTER_CLOCKWISE, GRANIT_CULL_MODE_NONE,
                                     GRANIT_POLYGON_MODE_FILL},
        depth,
        desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE ? desc.depth_bias
                                                                         : nullptr,
        color_blends, {desc.color_formats, static_cast<std::size_t>(desc.color_format_count)},
        desc.depth_stencil_format, desc.sample_count, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (pipeline_layouts_.find(desc.layout) == pipeline_layouts_.end() ||
        shaders_.find(desc.vertex_shader) == shaders_.end() ||
        shaders_.find(desc.fragment_shader) == shaders_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::pipeline, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      graphics_pipelines_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::pipeline, state->domain()));
      throw;
    }
    pipeline = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::create_webgpu_graphics_pipeline(
    granit_renderer renderer, granit_pipeline_layout layout_handle, granit_shader vertex_shader,
    granit_shader fragment_shader, granit_texture_format color_format,
    granit_graphics_pipeline& pipeline) {
  try {
    auto owner = acquire_backend(renderer);
    auto pipelines = std::dynamic_pointer_cast<backend_pipeline_renderer>(owner);
    if (!owner || !pipelines)
      return GRANIT_ERROR_INVALID_HANDLE;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> vertex;
    std::shared_ptr<shader_record> fragment;
    {
      std::lock_guard lock{mutex_};
      const auto layout_found = pipeline_layouts_.find(layout_handle);
      const auto vertex_found = shaders_.find(vertex_shader);
      const auto fragment_found = shaders_.find(fragment_shader);
      if (layout_found == pipeline_layouts_.end() || vertex_found == shaders_.end() ||
          fragment_found == shaders_.end() || layout_found->second->owner != owner ||
          vertex_found->second->owner != owner || fragment_found->second->owner != owner)
        return GRANIT_ERROR_INVALID_HANDLE;
      layout = layout_found->second;
      vertex = vertex_found->second;
      fragment = fragment_found->second;
    }
    auto record = std::make_shared<graphics_pipeline_record>();
    record->owner = owner;
    record->layout = layout;
    record->vertex_shader = vertex;
    record->fragment_shader = fragment;
    record->native = pipelines->allocate_graphics_pipeline_resource();
    if (!record->native)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto result = pipelines->create_graphics_pipeline(
        *record->native, *layout->native, *vertex->native, *fragment->native, color_format);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto handle = handles_.insert(record.get(), resource_type::pipeline, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    graphics_pipelines_.emplace(handle, std::move(record));
    pipeline = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_graphics_pipeline(granit_renderer renderer,
                                                           granit_graphics_pipeline pipeline) {
  std::shared_ptr<graphics_pipeline_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = backend_renderers_.find(renderer);
    if (state == backend_renderers_.end() ||
        handles_.find(pipeline, resource_type::pipeline, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = graphics_pipelines_.find(pipeline);
    if (found == graphics_pipelines_.end() || found->second->owner != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    graphics_pipelines_.erase(found);
    static_cast<void>(handles_.erase(pipeline, resource_type::pipeline, state->second->domain()));
  }
  const auto state = std::dynamic_pointer_cast<renderer_state>(record->owner);
  const auto serial = record->metadata.last_use_serial.load();
  if (state) {
    state->retire_resource(serial, retirement_order::dependent, std::move(record));
    static_cast<void>(state->collect_retired());
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_compute_pipeline(granit_renderer renderer,
                                                         const granit_compute_pipeline_desc& desc,
                                                         granit_compute_pipeline& pipeline) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> compute;
    {
      std::lock_guard lock{mutex_};
      const auto found = backend_renderers_.find(renderer);
      if (found == backend_renderers_.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      state = std::dynamic_pointer_cast<renderer_state>(found->second);
      if (!state)
        return GRANIT_ERROR_UNSUPPORTED;
      const auto layout_found = pipeline_layouts_.find(desc.layout);
      const auto compute_found = shaders_.find(desc.compute_shader);
      if (layout_found == pipeline_layouts_.end() || compute_found == shaders_.end() ||
          layout_found->second->owner != state || compute_found->second->owner != state ||
          compute_found->second->stage != GRANIT_SHADER_STAGE_COMPUTE)
        return GRANIT_ERROR_INVALID_HANDLE;
      layout = layout_found->second;
      compute = compute_found->second;
    }
    auto record = std::make_shared<compute_pipeline_record>();
    record->owner = state;
    record->resource_api = state;
    record->retirement = state;
    record->layout = layout;
    record->compute_shader = compute;
    record->native = record->resource_api->allocate_compute_pipeline_resource();
    const auto result = record->resource_api->create_compute_pipeline(
        *layout->native, *compute->native, compute->entry_point.c_str(), *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (pipeline_layouts_.find(desc.layout) == pipeline_layouts_.end() ||
        shaders_.find(desc.compute_shader) == shaders_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::compute_pipeline, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      compute_pipelines_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::compute_pipeline, state->domain()));
      throw;
    }
    pipeline = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_compute_pipeline(granit_renderer renderer,
                                                          granit_compute_pipeline pipeline) {
  std::shared_ptr<compute_pipeline_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = backend_renderers_.find(renderer);
    if (state == backend_renderers_.end() ||
        handles_.find(pipeline, resource_type::compute_pipeline, state->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = compute_pipelines_.find(pipeline);
    if (found == compute_pipelines_.end() || found->second->owner != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    compute_pipelines_.erase(found);
    static_cast<void>(
        handles_.erase(pipeline, resource_type::compute_pipeline, state->second->domain()));
  }
  const auto retirement = record->retirement;
  const auto serial = record->metadata.last_use_serial.load();
  retirement->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(retirement->collect_retired());
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
