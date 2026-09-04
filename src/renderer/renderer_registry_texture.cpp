// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "backend/diagnostics.h"
#include "core/texture_format.h"
#include "renderer/renderer_registry_helpers.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result renderer_registry::create_texture(granit_renderer renderer,
                                                const granit_texture_desc& desc,
                                                granit_texture& texture) {
  try {
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = interfaces->renderer;
    if ((owner->capabilities().framebuffer_sample_counts & desc.sample_count) == 0)
      return GRANIT_ERROR_UNSUPPORTED;
    const auto& resource_api = interfaces->resources;
    if (!resource_api)
      return GRANIT_ERROR_UNSUPPORTED;
    auto record = std::make_shared<texture_record>();
    record->owner = owner;
    record->resource_api = resource_api;
    record->retirement = interfaces->retirement;
    record->desc = desc;
    record->native = resource_api->allocate_texture_resource();
    const auto result = resource_api->create_texture(desc, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::texture, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      textures_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::texture, owner->domain()));
      throw;
    }
    texture = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::write_texture(granit_renderer renderer, granit_texture texture,
                                               const void* data, std::uint64_t size,
                                               const granit_texture_data_layout& layout,
                                               const granit_texture_write_region& region) {
  std::shared_ptr<texture_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = renderer_found->second;
    if (handles_.find(texture, resource_type::texture, owner->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->owner != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  const auto& desc = record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > size || required > size - layout.offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  return record->resource_api->upload_texture(
      *record->native, desc.format, static_cast<const unsigned char*>(data) + layout.offset,
      required, layout, region);
}

granit_result
renderer_registry::get_texture_readback_info(granit_renderer renderer, granit_texture texture,
                                             const granit_texture_write_region& region,
                                             granit_texture_readback_info& info) {
  std::shared_ptr<texture_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
        handles_.find(texture, resource_type::texture, found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  const auto& desc = record->desc;
  const auto bytes = depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes == 0)
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
  const auto row = uint64_t{region.width} * bytes;
  const auto images =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  if (row > UINT32_MAX || region.height > UINT32_MAX || images > UINT64_MAX / region.height ||
      images * region.height > UINT64_MAX / row)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  info.format = desc.format;
  info.width = region.width;
  info.height = region.height;
  info.depth = region.depth;
  info.array_layer_count = region.array_layer_count;
  info.bytes_per_row = static_cast<uint32_t>(row);
  info.rows_per_image = region.height;
  info.required_size = images * region.height * row;
  info.reserved[0] = 0;
  info.reserved[1] = 0;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_texture_view(granit_renderer renderer,
                                                     granit_texture texture,
                                                     const granit_texture_view_desc& desc,
                                                     granit_texture_view& view) {
  try {
    std::shared_ptr<backend_renderer> owner;
    std::shared_ptr<backend_resource_renderer> resource_api;
    std::shared_ptr<texture_record> parent;
    {
      std::lock_guard lock{mutex_};
      const auto renderer_found = backend_renderers_.find(renderer);
      if (renderer_found == backend_renderers_.end() ||
          handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      owner = renderer_found->second;
      const auto interfaces_found = backend_interfaces_.find(renderer);
      if (interfaces_found == backend_interfaces_.end())
        return GRANIT_ERROR_INTERNAL;
      resource_api = interfaces_found->second->resources;
      if (!resource_api)
        return GRANIT_ERROR_UNSUPPORTED;
      if (handles_.find(texture, resource_type::texture, owner->domain()) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      const auto found = textures_.find(texture);
      if (found == textures_.end() || found->second->owner != owner) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      parent = found->second;
    }
    if (desc.dimension != parent->desc.dimension ||
        (desc.format != GRANIT_TEXTURE_FORMAT_UNDEFINED && desc.format != parent->desc.format)) {
      return GRANIT_ERROR_UNSUPPORTED;
    }
    if (desc.range.base_mip_level >= parent->desc.mip_levels ||
        desc.range.mip_level_count > parent->desc.mip_levels - desc.range.base_mip_level ||
        desc.range.base_array_layer >= parent->desc.array_layers ||
        desc.range.array_layer_count > parent->desc.array_layers - desc.range.base_array_layer) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto aspect = desc.range.aspect;
    const auto depth = parent->desc.format >= GRANIT_TEXTURE_FORMAT_D16_UNORM;
    const auto stencil = parent->desc.format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
                         parent->desc.format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
    if (aspect != GRANIT_TEXTURE_ASPECT_AUTOMATIC &&
        ((!depth && aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT) ||
         (depth && (aspect & GRANIT_TEXTURE_ASPECT_COLOR_BIT) != 0) ||
         (depth && !stencil && aspect != GRANIT_TEXTURE_ASPECT_DEPTH_BIT) ||
         (depth && stencil && (aspect & GRANIT_TEXTURE_ASPECT_DEPTH_BIT) == 0 &&
          (aspect & GRANIT_TEXTURE_ASPECT_STENCIL_BIT) == 0))) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    auto record = std::make_shared<texture_view_record>();
    record->owner = owner;
    record->resource_api = resource_api;
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces || interfaces->renderer != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->retirement = interfaces->retirement;
    record->texture = parent;
    record->desc = desc;
    record->native = resource_api->allocate_texture_view_resource();
    const auto result =
        resource_api->create_texture_view(*parent->native, parent->desc, desc, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto parent_found = textures_.find(texture);
    if (parent_found == textures_.end() || parent_found->second != parent) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::texture_view, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      texture_views_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::texture_view, owner->domain()));
      throw;
    }
    view = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_texture_view(granit_renderer renderer,
                                                      granit_texture_view view) {
  std::shared_ptr<texture_view_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = renderer_found->second;
    if (handles_.find(view, resource_type::texture_view, owner->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = texture_views_.find(view);
    if (found == texture_views_.end() || found->second->owner != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (!found->second->publicly_destroyable)
      return GRANIT_ERROR_UNSUPPORTED;
    record = std::move(found->second);
    texture_views_.erase(found);
    static_cast<void>(handles_.erase(view, resource_type::texture_view, owner->domain()));
  }
  const auto retirement = record->retirement;
  if (retirement) {
    retirement->retire_resource(record->metadata.last_use_serial.load(),
                                retirement_order::dependent, record);
    record.reset();
    static_cast<void>(retirement->collect_retired());
  } else {
    record.reset();
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_texture(granit_renderer renderer, granit_texture texture) {
  std::shared_ptr<texture_record> record;
  std::vector<std::shared_ptr<texture_view_record>> views;
  std::shared_ptr<backend_diagnostic_renderer> diagnostics;
  lifecycle_snapshot lifecycle;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = renderer_found->second;
    const auto interfaces_found = backend_interfaces_.find(renderer);
    if (interfaces_found == backend_interfaces_.end())
      return GRANIT_ERROR_INTERNAL;
    diagnostics = interfaces_found->second->diagnostics;
    if (handles_.find(texture, resource_type::texture, owner->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->owner != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (!found->second->publicly_destroyable)
      return GRANIT_ERROR_UNSUPPORTED;
    for (auto view = texture_views_.begin(); view != texture_views_.end();) {
      if (view->second->texture == found->second) {
        if (diagnostics && diagnostics->validation_enabled() &&
            view->second->publicly_destroyable) {
          lifecycle.add(lifecycle_resource_type::texture_view, view->first,
                        view->second->metadata.creation_sequence);
        }
        views.push_back(std::move(view->second));
        static_cast<void>(
            handles_.erase(view->first, resource_type::texture_view, owner->domain()));
        view = texture_views_.erase(view);
      } else
        ++view;
    }
    record = std::move(found->second);
    textures_.erase(found);
    static_cast<void>(handles_.erase(texture, resource_type::texture, owner->domain()));
  }
  if (diagnostics)
    write_child_lifecycle_diagnostic(diagnostics->diagnostics(), lifecycle_resource_type::texture,
                                     texture, lifecycle_resource_type::texture_view,
                                     lifecycle.summary(lifecycle_resource_type::texture_view));
  const auto retirement = record->retirement;
  for (auto& view : views) {
    if (retirement)
      retirement->retire_resource(view->metadata.last_use_serial.load(),
                                  retirement_order::dependent, view);
  }
  views.clear();
  if (retirement)
    retirement->retire_resource(record->metadata.last_use_serial.load(), retirement_order::resource,
                                record);
  record.reset();
  if (retirement)
    static_cast<void>(retirement->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_sampler(granit_renderer renderer,
                                                const granit_sampler_desc& desc,
                                                granit_sampler& sampler) {
  try {
    const auto interfaces = acquire_backend_interfaces(renderer);
    if (!interfaces)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& owner = interfaces->renderer;
    if (desc.anisotropy_enabled != 0 &&
        desc.max_anisotropy > owner->capabilities().max_sampler_anisotropy)
      return GRANIT_ERROR_UNSUPPORTED;
    const auto& resource_api = interfaces->resources;
    if (!resource_api)
      return GRANIT_ERROR_UNSUPPORTED;
    auto record = std::make_shared<sampler_record>();
    record->owner = owner;
    record->resource_api = resource_api;
    record->retirement = interfaces->retirement;
    record->compare_operation = desc.compare_operation;
    record->native = record->resource_api->allocate_sampler_resource();
    const auto result = record->resource_api->create_sampler(desc, *record->native);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::sampler, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      samplers_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::sampler, owner->domain()));
      throw;
    }
    sampler = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_sampler(granit_renderer renderer, granit_sampler sampler) {
  std::shared_ptr<sampler_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(sampler, resource_type::sampler, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = samplers_.find(sampler);
    if (found == samplers_.end() || found->second->owner != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    samplers_.erase(found);
    static_cast<void>(handles_.erase(sampler, resource_type::sampler, state->domain()));
  }
  const auto retirement = record->retirement;
  const auto serial = record->metadata.last_use_serial.load();
  if (retirement) {
    retirement->retire_resource(serial, retirement_order::resource, std::move(record));
    static_cast<void>(retirement->collect_retired());
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
