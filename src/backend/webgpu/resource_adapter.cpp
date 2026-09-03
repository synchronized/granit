// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/resource_adapter.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

struct webgpu_resource_context {
  backend_plugin_loader* loader{};
  granit_backend_plugin_instance instance{};
};

namespace {

class webgpu_buffer_resource final : public backend_buffer_resource {
public:
  explicit webgpu_buffer_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_buffer_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_buffer(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_backend_plugin_buffer handle_{};
  granit_memory_location memory_location_{};
  std::vector<std::byte> host_memory_;
};

class webgpu_texture_resource final : public backend_texture_resource {
public:
  explicit webgpu_texture_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_texture_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_texture(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_backend_plugin_texture handle_{};
  granit_texture_format format_{GRANIT_TEXTURE_FORMAT_UNDEFINED};
};

class webgpu_texture_view_resource final : public backend_texture_view_resource {
public:
  explicit webgpu_texture_view_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_texture_view_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_texture_view(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_backend_plugin_texture_view handle_{};
};

class webgpu_sampler_resource final : public backend_sampler_resource {
public:
  explicit webgpu_sampler_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_sampler_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_sampler(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_backend_plugin_sampler handle_{};
};

class webgpu_bind_group_layout_resource final : public backend_bind_group_layout_resource {
public:
  explicit webgpu_bind_group_layout_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_bind_group_layout_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_bind_group_layout(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_backend_plugin_bind_group_layout handle_{};
};

class webgpu_bind_group_resource final : public backend_bind_group_resource {
public:
  explicit webgpu_bind_group_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_bind_group_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_bind_group(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_backend_plugin_bind_group handle_{};
};

webgpu_buffer_resource* as_buffer(backend_buffer_resource& resource) noexcept {
  return dynamic_cast<webgpu_buffer_resource*>(&resource);
}

const webgpu_buffer_resource* as_buffer(const backend_buffer_resource& resource) noexcept {
  return dynamic_cast<const webgpu_buffer_resource*>(&resource);
}

granit_backend_plugin_texture_format to_format(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_R8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RG8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_SRGB;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA16_FLOAT;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT;
  default:
    return 0;
  }
}

std::uint32_t bytes_per_pixel(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return 1;
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
    return 2;
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    return 4;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    return 8;
  default:
    return 0;
  }
}

granit_backend_plugin_texture_usage to_usage(granit_texture_usage usage) noexcept {
  granit_backend_plugin_texture_usage result{};
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_SAMPLED_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT;
  if ((usage & (GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
    result |= GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT;
  return result;
}

granit_backend_plugin_buffer_usage to_usage(granit_buffer_usage usage,
                                            granit_memory_location location) noexcept {
  granit_backend_plugin_buffer_usage result{};
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_VERTEX_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_INDEX_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_INDEX_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_UNIFORM_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_UNIFORM_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_STORAGE_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_STORAGE_BIT;
  if (location == GRANIT_MEMORY_LOCATION_READBACK)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
              GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  if (location == GRANIT_MEMORY_LOCATION_UPLOAD)
    result |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  return result;
}

} // namespace

webgpu_resource_adapter::webgpu_resource_adapter(backend_plugin_loader& loader,
                                                 granit_backend_plugin_instance instance)
    : context_(
          std::make_shared<webgpu_resource_context>(webgpu_resource_context{&loader, instance})) {}

std::unique_ptr<backend_buffer_resource> webgpu_resource_adapter::allocate_buffer() const {
  return std::make_unique<webgpu_buffer_resource>(context_);
}

granit_result
webgpu_resource_adapter::create_buffer(const granit_buffer_desc& desc,
                                       backend_buffer_resource& resource) const noexcept {
  auto* buffer = as_buffer(resource);
  const auto usage = to_usage(desc.usage, desc.memory_location);
  if (buffer == nullptr || buffer->handle_ != 0 || usage == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  if (desc.size > std::numeric_limits<std::size_t>::max())
    return GRANIT_ERROR_OUT_OF_MEMORY;
  try {
    if (desc.memory_location == GRANIT_MEMORY_LOCATION_UPLOAD ||
        desc.memory_location == GRANIT_MEMORY_LOCATION_READBACK)
      buffer->host_memory_.resize(static_cast<std::size_t>(desc.size));
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  granit_backend_plugin_buffer_desc plugin_desc{sizeof(plugin_desc), 0, desc.size, usage, 0};
  const auto result =
      context_->loader->create_buffer(context_->instance, &plugin_desc, &buffer->handle_);
  if (result == GRANIT_SUCCESS)
    buffer->memory_location_ = desc.memory_location;
  return result;
}

void* webgpu_resource_adapter::mapped_data(backend_buffer_resource& resource) const noexcept {
  auto* buffer = as_buffer(resource);
  return buffer == nullptr || buffer->host_memory_.empty() ? nullptr : buffer->host_memory_.data();
}

granit_result webgpu_resource_adapter::flush(backend_buffer_resource& resource,
                                             std::uint64_t offset,
                                             std::uint64_t size) const noexcept {
  auto* buffer = as_buffer(resource);
  if (buffer == nullptr || buffer->memory_location_ != GRANIT_MEMORY_LOCATION_UPLOAD ||
      offset > buffer->host_memory_.size() || size > buffer->host_memory_.size() - offset)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->write_buffer(context_->instance, buffer->handle_, offset,
                                        buffer->host_memory_.data() + offset, size);
}

granit_result webgpu_resource_adapter::invalidate(backend_buffer_resource& resource,
                                                  std::uint64_t offset,
                                                  std::uint64_t size) const noexcept {
  auto* buffer = as_buffer(resource);
  if (buffer == nullptr || buffer->memory_location_ != GRANIT_MEMORY_LOCATION_READBACK ||
      offset > buffer->host_memory_.size() || size > buffer->host_memory_.size() - offset)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->read_buffer(context_->instance, buffer->handle_, offset,
                                       buffer->host_memory_.data() + offset, size);
}

granit_result webgpu_resource_adapter::upload(backend_buffer_resource& resource,
                                              std::uint64_t offset, const void* data,
                                              std::uint64_t size) const noexcept {
  auto* buffer = as_buffer(resource);
  return buffer == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                           : context_->loader->write_buffer(context_->instance, buffer->handle_,
                                                            offset, data, size);
}

granit_result webgpu_resource_adapter::upload_batch(
    std::span<const backend_upload_operation> uploads) const noexcept {
  if (uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_backend_plugin_upload_operation> operations;
    operations.reserve(uploads.size());
    for (const auto& upload : uploads) {
      if (upload.data == nullptr || upload.size == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      granit_backend_plugin_upload_operation operation{};
      operation.struct_size = sizeof(operation);
      operation.data = upload.data;
      operation.size = upload.size;
      if (upload.type == backend_upload_type::buffer) {
        const auto* buffer = upload.buffer == nullptr ? nullptr : as_buffer(*upload.buffer);
        if (buffer == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        operation.type = GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_BUFFER;
        operation.buffer = buffer->handle_;
        operation.destination_offset = upload.destination_offset;
      } else if (upload.type == backend_upload_type::texture) {
        const auto* texture = upload.texture == nullptr
                                  ? nullptr
                                  : dynamic_cast<const webgpu_texture_resource*>(upload.texture);
        if (texture == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        const auto& copy = upload.texture_copy;
        const auto pixel_size = bytes_per_pixel(texture->format_);
        if (pixel_size == 0 || copy.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT ||
            copy.base_array_layer != 0 || copy.array_layer_count != 1 || copy.z != 0 ||
            copy.depth != 1 || copy.x < 0 || copy.y < 0 ||
            (copy.buffer_row_length != 0 && copy.buffer_row_length > UINT32_MAX / pixel_size))
          return GRANIT_ERROR_UNSUPPORTED;
        operation.type = GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_TEXTURE;
        operation.texture = texture->handle_;
        operation.texture_write = {
            sizeof(granit_backend_plugin_texture_write_desc),
            copy.mip_level,
            static_cast<std::uint32_t>(copy.x),
            static_cast<std::uint32_t>(copy.y),
            copy.width,
            copy.height,
            copy.buffer_row_length == 0 ? 0 : copy.buffer_row_length * pixel_size,
            copy.buffer_image_height,
            copy.base_array_layer,
            copy.array_layer_count};
      } else {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      operations.push_back(operation);
    }
    return context_->loader->write_upload_batch(context_->instance, operations);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_backend_plugin_buffer
webgpu_resource_adapter::native_buffer(backend_buffer_resource& resource) const noexcept {
  const auto* buffer = as_buffer(resource);
  return buffer == nullptr ? 0 : buffer->handle_;
}

std::unique_ptr<backend_texture_resource> webgpu_resource_adapter::allocate_texture() const {
  return std::make_unique<webgpu_texture_resource>(context_);
}

granit_result
webgpu_resource_adapter::create_texture(const granit_texture_desc& desc,
                                        backend_texture_resource& resource) const noexcept {
  auto* texture = dynamic_cast<webgpu_texture_resource*>(&resource);
  const auto format = to_format(desc.format);
  const auto usage = to_usage(desc.usage);
  if (texture == nullptr || texture->handle_ != 0 || format == 0 || usage == 0 ||
      (desc.dimension != GRANIT_TEXTURE_DIMENSION_2D &&
       desc.dimension != GRANIT_TEXTURE_DIMENSION_CUBE) ||
      desc.depth != 1 ||
      (desc.sample_count != GRANIT_SAMPLE_COUNT_1 && desc.sample_count != GRANIT_SAMPLE_COUNT_4))
    return GRANIT_ERROR_UNSUPPORTED;
  const granit_backend_plugin_texture_desc plugin_desc{
      sizeof(plugin_desc),
      0,
      desc.width,
      desc.height,
      usage,
      format,
      desc.mip_levels,
      desc.dimension == GRANIT_TEXTURE_DIMENSION_CUBE ? GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE
                                                      : GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D,
      desc.array_layers,
      desc.sample_count};
  const auto result =
      context_->loader->create_texture(context_->instance, &plugin_desc, &texture->handle_);
  if (result == GRANIT_SUCCESS)
    texture->format_ = desc.format;
  return result;
}

granit_backend_plugin_texture
webgpu_resource_adapter::native_texture(backend_texture_resource& resource) const noexcept {
  const auto* texture = dynamic_cast<webgpu_texture_resource*>(&resource);
  return texture == nullptr ? 0 : texture->handle_;
}

granit_result
webgpu_resource_adapter::upload_texture(backend_texture_resource& resource, const void* data,
                                        std::uint64_t size,
                                        const granit_texture_data_layout& layout,
                                        const granit_texture_write_region& region) const noexcept {
  auto* texture = dynamic_cast<webgpu_texture_resource*>(&resource);
  if (texture == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const granit_backend_plugin_texture_write_desc desc{
      sizeof(granit_backend_plugin_texture_write_desc),
      region.mip_level,
      region.x,
      region.y,
      region.width,
      region.height,
      layout.bytes_per_row,
      layout.rows_per_image,
      region.base_array_layer,
      region.array_layer_count};
  return context_->loader->write_texture(context_->instance, texture->handle_, &desc, data, size);
}

std::unique_ptr<backend_texture_view_resource>
webgpu_resource_adapter::allocate_texture_view() const {
  return std::make_unique<webgpu_texture_view_resource>(context_);
}

granit_result webgpu_resource_adapter::create_texture_view(
    backend_texture_resource& texture, const granit_texture_desc& texture_desc,
    const granit_texture_view_desc& desc, backend_texture_view_resource& resource) const noexcept {
  auto* native_texture = dynamic_cast<webgpu_texture_resource*>(&texture);
  auto* view = dynamic_cast<webgpu_texture_view_resource*>(&resource);
  const auto format =
      to_format(desc.format == GRANIT_TEXTURE_FORMAT_UNDEFINED ? texture_desc.format : desc.format);
  const auto mip_count = desc.range.mip_level_count == GRANIT_REMAINING_MIP_LEVELS
                             ? texture_desc.mip_levels - desc.range.base_mip_level
                             : desc.range.mip_level_count;
  if (native_texture == nullptr || view == nullptr || view->handle_ != 0 || format == 0 ||
      (desc.dimension != GRANIT_TEXTURE_DIMENSION_2D &&
       desc.dimension != GRANIT_TEXTURE_DIMENSION_CUBE))
    return GRANIT_ERROR_UNSUPPORTED;
  const granit_backend_plugin_texture_view_desc plugin_desc{
      sizeof(plugin_desc),
      format,
      desc.range.base_mip_level,
      mip_count,
      desc.dimension == GRANIT_TEXTURE_DIMENSION_CUBE ? GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE
                                                      : GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D,
      desc.range.base_array_layer,
      desc.range.array_layer_count};
  return context_->loader->create_texture_view(context_->instance, native_texture->handle_,
                                               &plugin_desc, &view->handle_);
}

granit_backend_plugin_texture_view webgpu_resource_adapter::native_texture_view(
    backend_texture_view_resource& resource) const noexcept {
  const auto* view = dynamic_cast<webgpu_texture_view_resource*>(&resource);
  return view == nullptr ? 0 : view->handle_;
}

std::unique_ptr<backend_sampler_resource> webgpu_resource_adapter::allocate_sampler() const {
  return std::make_unique<webgpu_sampler_resource>(context_);
}

granit_result
webgpu_resource_adapter::create_sampler(const granit_sampler_desc& desc,
                                        backend_sampler_resource& resource) const noexcept {
  auto* sampler = dynamic_cast<webgpu_sampler_resource*>(&resource);
  if (sampler == nullptr || sampler->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc.lod_bias != 0.0F || desc.max_anisotropy > UINT16_MAX ||
      std::floor(desc.max_anisotropy) != desc.max_anisotropy ||
      (desc.max_anisotropy > 1.0F &&
       (desc.min_filter != GRANIT_FILTER_LINEAR || desc.mag_filter != GRANIT_FILTER_LINEAR ||
        desc.mipmap_filter != GRANIT_MIPMAP_FILTER_LINEAR)))
    return GRANIT_ERROR_UNSUPPORTED;
  const granit_backend_plugin_sampler_desc plugin_desc{
      sizeof(granit_backend_plugin_sampler_desc),
      0,
      desc.min_filter + 1,
      desc.mag_filter + 1,
      desc.mipmap_filter + 1,
      desc.address_mode_u + 1,
      desc.address_mode_v + 1,
      desc.address_mode_w + 1,
      desc.compare_operation,
      static_cast<std::uint32_t>(desc.max_anisotropy),
      desc.min_lod,
      desc.max_lod,
      {0, 0}};
  return context_->loader->create_sampler(context_->instance, &plugin_desc, &sampler->handle_);
}

std::unique_ptr<backend_bind_group_layout_resource>
webgpu_resource_adapter::allocate_bind_group_layout() const {
  return std::make_unique<webgpu_bind_group_layout_resource>(context_);
}

granit_result webgpu_resource_adapter::create_bind_group_layout(
    std::span<const granit_bind_group_layout_entry> entries,
    backend_bind_group_layout_resource& resource) const noexcept {
  auto* layout = dynamic_cast<webgpu_bind_group_layout_resource*>(&resource);
  if (layout == nullptr || layout->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<granit_backend_plugin_bind_group_layout_entry> plugin_entries;
  try {
    plugin_entries.reserve(entries.size());
    for (const auto& entry : entries) {
      granit_backend_plugin_binding_type type{};
      switch (entry.type) {
      case GRANIT_BINDING_TYPE_UNIFORM_BUFFER:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_UNIFORM_BUFFER;
        break;
      case GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        break;
      case GRANIT_BINDING_TYPE_STORAGE_BUFFER:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_STORAGE_BUFFER;
        break;
      case GRANIT_BINDING_TYPE_SAMPLED_TEXTURE:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE;
        break;
      case GRANIT_BINDING_TYPE_SAMPLED_TEXTURE_CUBE:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE_CUBE;
        break;
      case GRANIT_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE;
        break;
      case GRANIT_BINDING_TYPE_SAMPLER:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER;
        break;
      case GRANIT_BINDING_TYPE_COMPARISON_SAMPLER:
        type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_COMPARISON_SAMPLER;
        break;
      default:
        return GRANIT_ERROR_UNSUPPORTED;
      }
      if (entry.array_count != 1)
        return GRANIT_ERROR_UNSUPPORTED;
      plugin_entries.push_back({entry.binding, type, entry.visibility, entry.array_count});
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  const granit_backend_plugin_bind_group_layout_desc desc{
      sizeof(granit_backend_plugin_bind_group_layout_desc),
      static_cast<std::uint32_t>(plugin_entries.size()), plugin_entries.data(), 0};
  return context_->loader->create_bind_group_layout(context_->instance, &desc, &layout->handle_);
}

std::unique_ptr<backend_bind_group_resource> webgpu_resource_adapter::allocate_bind_group() const {
  return std::make_unique<webgpu_bind_group_resource>(context_);
}

granit_result
webgpu_resource_adapter::create_bind_group(backend_bind_group_layout_resource& layout,
                                           std::span<const backend_bind_group_write> writes,
                                           backend_bind_group_resource& resource) const noexcept {
  auto* native_layout = dynamic_cast<webgpu_bind_group_layout_resource*>(&layout);
  auto* group = dynamic_cast<webgpu_bind_group_resource*>(&resource);
  if (native_layout == nullptr || group == nullptr || group->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<granit_backend_plugin_bind_group_entry> entries;
  try {
    entries.reserve(writes.size());
    for (const auto& write : writes) {
      granit_backend_plugin_bind_group_entry entry{};
      entry.binding = write.binding;
      entry.offset = write.offset;
      entry.size = write.range;
      switch (write.type) {
      case backend_binding_type::uniform_buffer:
        entry.type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_UNIFORM_BUFFER;
        entry.buffer = native_buffer(*write.buffer);
        break;
      case backend_binding_type::dynamic_uniform_buffer:
        entry.type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        entry.buffer = native_buffer(*write.buffer);
        break;
      case backend_binding_type::storage_buffer:
        entry.type = GRANIT_BACKEND_PLUGIN_BINDING_TYPE_STORAGE_BUFFER;
        entry.buffer = native_buffer(*write.buffer);
        break;
      case backend_binding_type::sampled_texture:
      case backend_binding_type::sampled_texture_cube:
      case backend_binding_type::sampled_depth_texture: {
        const auto* view = dynamic_cast<webgpu_texture_view_resource*>(write.texture_view);
        if (view == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.type = write.type == backend_binding_type::sampled_texture_cube
                         ? GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE_CUBE
                     : write.type == backend_binding_type::sampled_depth_texture
                         ? GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE
                         : GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE;
        entry.texture_view = view->handle_;
        break;
      }
      case backend_binding_type::sampler:
      case backend_binding_type::comparison_sampler: {
        const auto* sampler = dynamic_cast<webgpu_sampler_resource*>(write.sampler);
        if (sampler == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.type = write.type == backend_binding_type::comparison_sampler
                         ? GRANIT_BACKEND_PLUGIN_BINDING_TYPE_COMPARISON_SAMPLER
                         : GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER;
        entry.sampler = sampler->handle_;
        break;
      }
      default:
        return GRANIT_ERROR_UNSUPPORTED;
      }
      entries.push_back(entry);
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  const granit_backend_plugin_bind_group_desc desc{sizeof(granit_backend_plugin_bind_group_desc),
                                                   static_cast<std::uint32_t>(entries.size()),
                                                   native_layout->handle_, entries.data(), 0};
  return context_->loader->create_bind_group(context_->instance, &desc, &group->handle_);
}

granit_backend_plugin_bind_group_layout webgpu_resource_adapter::native_bind_group_layout(
    backend_bind_group_layout_resource& resource) const noexcept {
  const auto* layout = dynamic_cast<webgpu_bind_group_layout_resource*>(&resource);
  return layout == nullptr ? 0 : layout->handle_;
}

granit_backend_plugin_bind_group
webgpu_resource_adapter::native_bind_group(backend_bind_group_resource& resource) const noexcept {
  const auto* group = dynamic_cast<webgpu_bind_group_resource*>(&resource);
  return group == nullptr ? 0 : group->handle_;
}

} // namespace granit::detail
