// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "renderer/renderer_registry_helpers.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result renderer_registry::bind_graphics_pipeline(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_graphics_pipeline pipeline) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<graphics_pipeline_record> pipeline_record;
  {
    std::lock_guard lock{mutex_};
    const auto found = graphics_pipelines_.find(pipeline);
    if (found == graphics_pipelines_.end() || found->second->owner != command->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    pipeline_record = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (command->platform_managed_rendering) {
    if (command->web_status != command_recorder_record::web_state::rendering)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    command->web_pipeline = pipeline_record;
    return GRANIT_SUCCESS;
  }
  if (!command->commands->command_recorder_is_recording(*command->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      command->graphics->bind_graphics_pipeline(*command->native, *pipeline_record->native);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, pipeline_record, pipeline_record->metadata);
  return result;
}

granit_result
renderer_registry::bind_graphics_groups(granit_renderer renderer, granit_command_recorder recorder,
                                        granit_pipeline_layout layout, std::uint32_t first_group,
                                        std::span<const granit_bind_group> bind_groups,
                                        std::span<const std::uint32_t> dynamic_offsets) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<pipeline_layout_record> layout_record;
  std::vector<std::shared_ptr<bind_group_record>> group_records;
  std::vector<backend_bind_group_resource*> native_groups;
  std::vector<backend_buffer_access> buffer_accesses;
  std::vector<backend_texture_access> texture_accesses;
  std::vector<dynamic_uniform_binding> dynamic_bindings;
  {
    std::lock_guard lock{mutex_};
    const auto layout_found = pipeline_layouts_.find(layout);
    if (layout_found == pipeline_layouts_.end() || layout_found->second->owner != command->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    layout_record = layout_found->second;
    if (first_group + bind_groups.size() > layout_record->bind_group_layouts.size())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    group_records.reserve(bind_groups.size());
    native_groups.reserve(bind_groups.size());
    for (std::size_t index = 0; index < bind_groups.size(); ++index) {
      const auto found = bind_groups_.find(bind_groups[index]);
      if (found == bind_groups_.end() || found->second->owner != command->owner)
        return GRANIT_ERROR_INVALID_HANDLE;
      if (found->second->layout != layout_record->bind_group_layouts[first_group + index])
        return GRANIT_ERROR_INVALID_ARGUMENT;
      group_records.push_back(found->second);
      native_groups.push_back(found->second->native.get());
      buffer_accesses.insert(buffer_accesses.end(), found->second->graphics_buffer_accesses.begin(),
                             found->second->graphics_buffer_accesses.end());
      texture_accesses.insert(texture_accesses.end(),
                              found->second->graphics_texture_accesses.begin(),
                              found->second->graphics_texture_accesses.end());
      dynamic_bindings.insert(dynamic_bindings.end(),
                              found->second->dynamic_uniform_bindings.begin(),
                              found->second->dynamic_uniform_bindings.end());
    }
  }
  if (!validate_dynamic_uniform_offsets(
          dynamic_bindings, dynamic_offsets,
          command->owner->capabilities().uniform_buffer_offset_alignment))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard command_lock{command->mutex};
  if (!command->commands->command_recorder_is_recording(*command->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command->graphics->bind_graphics_groups(
      *command->native, *layout_record->native, first_group, native_groups, dynamic_offsets,
      buffer_accesses, texture_accesses);
  if (result == GRANIT_SUCCESS) {
    retain_resource(command->retained_resources, layout_record, layout_record->metadata);
    for (const auto& group : group_records)
      retain_resource(command->retained_resources, group, group->metadata);
  }
  return result;
}

granit_result renderer_registry::bind_compute_pipeline(granit_renderer renderer,
                                                       granit_command_recorder recorder,
                                                       granit_compute_pipeline pipeline) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!command->compute)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<compute_pipeline_record> pipeline_record;
  {
    std::lock_guard lock{mutex_};
    const auto found = compute_pipelines_.find(pipeline);
    if (found == compute_pipelines_.end() || found->second->owner != command->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    pipeline_record = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (!command->commands->command_recorder_is_recording(*command->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      command->compute->bind_compute_pipeline(*command->native, *pipeline_record->native);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, pipeline_record, pipeline_record->metadata);
  return result;
}

granit_result
renderer_registry::bind_compute_groups(granit_renderer renderer, granit_command_recorder recorder,
                                       granit_pipeline_layout layout, std::uint32_t first_group,
                                       std::span<const granit_bind_group> bind_groups,
                                       std::span<const std::uint32_t> dynamic_offsets) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!command->compute)
    return GRANIT_ERROR_UNSUPPORTED;
  std::shared_ptr<pipeline_layout_record> layout_record;
  std::vector<std::shared_ptr<bind_group_record>> group_records;
  std::vector<backend_bind_group_resource*> native_groups;
  std::vector<backend_buffer_access> buffer_accesses;
  std::vector<backend_texture_access> texture_accesses;
  std::vector<dynamic_uniform_binding> dynamic_bindings;
  {
    std::lock_guard lock{mutex_};
    const auto layout_found = pipeline_layouts_.find(layout);
    if (layout_found == pipeline_layouts_.end() || layout_found->second->owner != command->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    layout_record = layout_found->second;
    if (first_group + bind_groups.size() > layout_record->bind_group_layouts.size())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    group_records.reserve(bind_groups.size());
    native_groups.reserve(bind_groups.size());
    for (std::size_t index = 0; index < bind_groups.size(); ++index) {
      const auto found = bind_groups_.find(bind_groups[index]);
      if (found == bind_groups_.end() || found->second->owner != command->owner)
        return GRANIT_ERROR_INVALID_HANDLE;
      if (found->second->layout != layout_record->bind_group_layouts[first_group + index])
        return GRANIT_ERROR_INVALID_ARGUMENT;
      group_records.push_back(found->second);
      native_groups.push_back(found->second->native.get());
      buffer_accesses.insert(buffer_accesses.end(), found->second->compute_buffer_accesses.begin(),
                             found->second->compute_buffer_accesses.end());
      texture_accesses.insert(texture_accesses.end(),
                              found->second->compute_texture_accesses.begin(),
                              found->second->compute_texture_accesses.end());
      dynamic_bindings.insert(dynamic_bindings.end(),
                              found->second->dynamic_uniform_bindings.begin(),
                              found->second->dynamic_uniform_bindings.end());
    }
  }
  if (!validate_dynamic_uniform_offsets(
          dynamic_bindings, dynamic_offsets,
          command->owner->capabilities().uniform_buffer_offset_alignment))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard command_lock{command->mutex};
  if (!command->commands->command_recorder_is_recording(*command->native))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command->compute->bind_compute_groups(
      *command->native, *layout_record->native, first_group, native_groups, dynamic_offsets,
      buffer_accesses, texture_accesses);
  if (result == GRANIT_SUCCESS) {
    retain_resource(command->retained_resources, layout_record, layout_record->metadata);
    for (const auto& group : group_records)
      retain_resource(command->retained_resources, group, group->metadata);
  }
  return result;
}

granit_result renderer_registry::dispatch(granit_renderer renderer,
                                          granit_command_recorder recorder,
                                          std::uint32_t group_count_x, std::uint32_t group_count_y,
                                          std::uint32_t group_count_z) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!command->compute)
    return GRANIT_ERROR_UNSUPPORTED;
  std::lock_guard lock{command->mutex};
  return command->compute->dispatch(*command->native, group_count_x, group_count_y, group_count_z);
}

granit_result renderer_registry::set_viewports(granit_renderer renderer,
                                               granit_command_recorder recorder,
                                               std::uint32_t first,
                                               std::span<const granit_viewport> viewports) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->graphics->set_viewports(*command->native, first, viewports);
}

granit_result renderer_registry::set_scissors(granit_renderer renderer,
                                              granit_command_recorder recorder, std::uint32_t first,
                                              std::span<const granit_scissor> scissors) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->graphics->set_scissors(*command->native, first, scissors);
}

granit_result
renderer_registry::bind_vertex_buffers(granit_renderer renderer, granit_command_recorder recorder,
                                       std::uint32_t first,
                                       std::span<const granit_vertex_buffer_binding> bindings) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::vector<std::shared_ptr<buffer_record>> records;
  std::vector<backend_buffer_resource*> buffers;
  std::vector<std::uint64_t> offsets;
  {
    std::lock_guard lock{mutex_};
    for (const auto& binding : bindings) {
      const auto found = buffers_.find(binding.buffer);
      if (found == buffers_.end() || found->second->owner != command->owner)
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((found->second->desc.usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) == 0 ||
          binding.offset >= found->second->desc.size)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      records.push_back(found->second);
      buffers.push_back(found->second->native.get());
      offsets.push_back(binding.offset);
    }
  }
  std::lock_guard lock{command->mutex};
  const auto result =
      command->graphics->bind_vertex_buffers(*command->native, first, buffers, offsets);
  if (result == GRANIT_SUCCESS) {
    for (const auto& record : records)
      retain_resource(command->retained_resources, record, record->metadata);
  }
  return result;
}

granit_result renderer_registry::bind_index_buffer(granit_renderer renderer,
                                                   granit_command_recorder recorder,
                                                   granit_buffer buffer, std::uint64_t offset,
                                                   granit_index_type type) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->owner != command->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto alignment = type == GRANIT_INDEX_TYPE_UINT16 ? 2U : 4U;
    if ((found->second->desc.usage & GRANIT_BUFFER_USAGE_INDEX_BIT) == 0 ||
        offset >= found->second->desc.size || offset % alignment != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    record = found->second;
  }
  std::lock_guard lock{command->mutex};
  const auto result =
      command->graphics->bind_index_buffer(*command->native, *record->native, offset, type);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, record, record->metadata);
  return result;
}

granit_result renderer_registry::draw(granit_renderer renderer, granit_command_recorder recorder,
                                      std::uint32_t vertex_count, std::uint32_t instance_count,
                                      std::uint32_t first_vertex, std::uint32_t first_instance) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  if (command->platform_managed_rendering) {
    if (command->web_status != command_recorder_record::web_state::rendering ||
        !command->web_target || !command->web_pipeline)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto result = command->graphics->draw(*command->native, command->web_target->native.get(),
                                                command->web_pipeline->native.get(), vertex_count,
                                                instance_count, first_vertex, first_instance);
    if (result == GRANIT_SUCCESS)
      command->web_drew = true;
    return result;
  }
  return command->graphics->draw(*command->native, nullptr, nullptr, vertex_count, instance_count,
                                 first_vertex, first_instance);
}

granit_result renderer_registry::draw_indexed(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              std::uint32_t index_count,
                                              std::uint32_t instance_count,
                                              std::uint32_t first_index, std::int32_t vertex_offset,
                                              std::uint32_t first_instance) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  if (command->platform_managed_rendering) {
    if (command->web_status != command_recorder_record::web_state::rendering ||
        !command->web_target || !command->web_pipeline)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto result = command->graphics->draw_indexed(
        *command->native, command->web_target->native.get(), command->web_pipeline->native.get(),
        index_count, instance_count, first_index, vertex_offset, first_instance);
    if (result == GRANIT_SUCCESS)
      command->web_drew = true;
    return result;
  }
  return command->graphics->draw_indexed(*command->native, nullptr, nullptr, index_count,
                                         instance_count, first_index, vertex_offset,
                                         first_instance);
}

granit_result renderer_registry::begin_rendering(granit_renderer renderer,
                                                 granit_command_recorder recorder,
                                                 const granit_rendering_desc& desc) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->platform_managed_rendering) {
    if (desc.color_attachment_count != 1 || desc.color_attachments == nullptr ||
        desc.depth_stencil_attachment != nullptr)
      return GRANIT_ERROR_UNSUPPORTED;
    std::shared_ptr<texture_view_record> view;
    {
      std::lock_guard lock{mutex_};
      const auto found = texture_views_.find(desc.color_attachments[0].view);
      if (found == texture_views_.end() || found->second->owner != command->owner)
        return GRANIT_ERROR_INVALID_HANDLE;
      view = found->second;
    }
    std::lock_guard command_lock{command->mutex};
    if (command->web_status != command_recorder_record::web_state::recording)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    command->web_target = std::move(view);
    command->web_status = command_recorder_record::web_state::rendering;
    return GRANIT_SUCCESS;
  }
  std::vector<std::shared_ptr<texture_view_record>> views;
  views.reserve(desc.color_attachment_count + (desc.depth_stencil_attachment ? 1U : 0U));
  {
    std::lock_guard lock{mutex_};
    const auto acquire_view = [&](granit_texture_view handle) {
      if (handles_.find(handle, resource_type::texture_view, command->owner->domain()) == nullptr)
        return std::shared_ptr<texture_view_record>{};
      const auto found = texture_views_.find(handle);
      return found != texture_views_.end() && found->second->owner == command->owner
                 ? found->second
                 : std::shared_ptr<texture_view_record>{};
    };
    for (std::uint32_t index = 0; index < desc.color_attachment_count; ++index) {
      auto view = acquire_view(desc.color_attachments[index].view);
      if (!view)
        return GRANIT_ERROR_INVALID_HANDLE;
      views.push_back(std::move(view));
    }
    if (desc.depth_stencil_attachment) {
      auto view = acquire_view(desc.depth_stencil_attachment->view);
      if (!view)
        return GRANIT_ERROR_INVALID_HANDLE;
      views.push_back(std::move(view));
    }
  }
  std::uint32_t width{}, height{};
  granit_sample_count samples{};
  for (std::size_t index = 0; index < views.size(); ++index) {
    const bool depth = index >= desc.color_attachment_count;
    const auto& texture = views[index]->texture->desc;
    const auto usage = depth ? GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                             : GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
    if (depth_format(texture.format) != depth || (texture.usage & usage) == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (width == 0) {
      width = texture.width;
      height = texture.height;
      samples = texture.sample_count;
    } else if (width != texture.width || height != texture.height ||
               samples != texture.sample_count) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    if (std::find(views.begin(), views.begin() + static_cast<std::ptrdiff_t>(index),
                  views[index]) != views.begin() + static_cast<std::ptrdiff_t>(index))
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.area.x > width || desc.area.width > width - desc.area.x || desc.area.y > height ||
      desc.area.height > height - desc.area.y)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<backend_color_attachment> colors;
  colors.reserve(desc.color_attachment_count);
  for (std::uint32_t index = 0; index < desc.color_attachment_count; ++index) {
    const auto& source = desc.color_attachments[index];
    const auto& view = views[index];
    colors.push_back({
        .texture = view->texture->native.get(),
        .view = view->native.get(),
        .range = view->desc.range,
        .format = view->texture->desc.format,
        .load_operation = source.load_operation,
        .store_operation = source.store_operation,
        .clear_value = source.clear_value,
    });
  }
  backend_depth_stencil_attachment depth_stencil{};
  const backend_depth_stencil_attachment* depth_stencil_ptr = nullptr;
  if (desc.depth_stencil_attachment) {
    const auto& source = *desc.depth_stencil_attachment;
    const auto& view = views.back();
    if (!stencil_format(view->texture->desc.format) &&
        (source.stencil_load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD ||
         source.stencil_store_operation != GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD)) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    depth_stencil = {
        .texture = view->texture->native.get(),
        .view = view->native.get(),
        .range = view->desc.range,
        .format = view->texture->desc.format,
        .depth_load_operation = source.depth_load_operation,
        .depth_store_operation = source.depth_store_operation,
        .stencil_load_operation = source.stencil_load_operation,
        .stencil_store_operation = source.stencil_store_operation,
        .clear_value = source.clear_value,
    };
    depth_stencil_ptr = &depth_stencil;
  }
  std::lock_guard command_lock{command->mutex};
  for (const auto& view : views)
    retain_resource(command->retained_resources, view, view->metadata);
  return command->graphics->begin_rendering(*command->native, desc.area, colors, depth_stencil_ptr,
                                            desc.layer_count);
}

granit_result renderer_registry::end_rendering(granit_renderer renderer,
                                               granit_command_recorder recorder) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  if (command->platform_managed_rendering) {
    if (command->web_status != command_recorder_record::web_state::rendering)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    command->web_status = command_recorder_record::web_state::recording;
    return GRANIT_SUCCESS;
  }
  return command->graphics->end_rendering(*command->native);
}

granit_result renderer_registry::destroy_command_recorder(granit_renderer renderer,
                                                          granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (record->owned_by_frame_context)
    return GRANIT_ERROR_UNSUPPORTED;
  {
    std::lock_guard record_lock{record->mutex};
    const auto wait_result = record->queue->wait_command_recorder(*record->native);
    if (wait_result != GRANIT_SUCCESS && wait_result != GRANIT_ERROR_DEVICE_LOST) {
      return wait_result;
    }
  }
  {
    std::lock_guard lock{mutex_};
    const auto found = command_recorders_.find(recorder);
    if (found == command_recorders_.end() || found->second != record ||
        handles_.find(recorder, resource_type::command_recorder, record->owner->domain()) ==
            nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    command_recorders_.erase(found);
    static_cast<void>(
        handles_.erase(recorder, resource_type::command_recorder, record->owner->domain()));
  }
  auto retirement = record->retirement;
  record.reset();
  if (retirement)
    static_cast<void>(retirement->collect_retired());
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
