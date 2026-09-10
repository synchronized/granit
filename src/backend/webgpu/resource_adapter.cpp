// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/resource_adapter.h"

#include "core/texture_format.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

struct webgpu_resource_context {
  webgpu_context* provider{};
  granit_webgpu_provider_instance instance{};
};

namespace {

class completed_upload final : public backend_upload_completion {
public:
  [[nodiscard]] granit_result poll() noexcept override { return GRANIT_SUCCESS; }
};

struct webgpu_readback_slice {
  std::uint64_t source_offset{};
  std::uint64_t result_size{};
  std::uint32_t source_bytes_per_row{};
  std::uint32_t result_bytes_per_row{};
  std::uint32_t rows{};
  std::uint32_t layers{};
  granit_readback_result_info info = GRANIT_READBACK_RESULT_INFO_INIT;
};

class webgpu_readback_completion final : public backend_readback_completion {
public:
  webgpu_readback_completion(std::shared_ptr<webgpu_resource_context> context,
                             granit_webgpu_provider_buffer buffer,
                             granit_webgpu_provider_readback readback,
                             std::vector<webgpu_readback_slice> slices)
      : context_(std::move(context)), buffer_(buffer), readback_(readback),
        slices_(std::move(slices)) {}
  ~webgpu_readback_completion() override {
    if (readback_ != 0)
      static_cast<void>(context_->provider->destroy_readback(context_->instance, readback_));
    if (buffer_ != 0)
      static_cast<void>(context_->provider->destroy_buffer(context_->instance, buffer_));
  }
  [[nodiscard]] granit_result poll() noexcept override {
    return context_->provider->poll_readback(context_->instance, readback_);
  }
  [[nodiscard]] granit_result
  get_result_info(std::uint32_t index, granit_readback_result_info& info) const noexcept override {
    if (index >= slices_.size())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    info = slices_[index].info;
    return GRANIT_SUCCESS;
  }
  [[nodiscard]] granit_result copy_result(std::uint32_t index, void* data,
                                          std::uint64_t size) noexcept override {
    if (index >= slices_.size() || data == nullptr || size != slices_[index].result_size)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto& slice = slices_[index];
    if (slice.source_bytes_per_row == slice.result_bytes_per_row)
      return context_->provider->copy_readback(context_->instance, readback_, slice.source_offset,
                                               data, size);
    try {
      std::vector<std::byte> padded(static_cast<std::size_t>(slice.source_bytes_per_row) *
                                    slice.rows * slice.layers);
      auto result = context_->provider->copy_readback(
          context_->instance, readback_, slice.source_offset, padded.data(), padded.size());
      if (result != GRANIT_SUCCESS)
        return result;
      auto* destination = static_cast<std::byte*>(data);
      for (std::uint32_t layer = 0; layer < slice.layers; ++layer) {
        for (std::uint32_t row = 0; row < slice.rows; ++row) {
          const auto source_offset =
              (static_cast<std::uint64_t>(layer) * slice.rows + row) * slice.source_bytes_per_row;
          const auto destination_offset =
              (static_cast<std::uint64_t>(layer) * slice.rows + row) * slice.result_bytes_per_row;
          std::memcpy(destination + destination_offset, padded.data() + source_offset,
                      slice.result_bytes_per_row);
        }
      }
      return GRANIT_SUCCESS;
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
  }

private:
  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_buffer buffer_{};
  granit_webgpu_provider_readback readback_{};
  std::vector<webgpu_readback_slice> slices_;
};

} // namespace

namespace {

class webgpu_buffer_resource final : public backend_buffer_resource {
public:
  explicit webgpu_buffer_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_buffer_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->provider->destroy_buffer(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_buffer handle_{};
  std::uint64_t size_{};
  granit_memory_location memory_location_{};
  std::vector<std::byte> host_memory_;
};

class webgpu_texture_resource final : public backend_texture_resource {
public:
  explicit webgpu_texture_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_texture_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->provider->destroy_texture(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_texture handle_{};
  granit_texture_format format_{GRANIT_TEXTURE_FORMAT_UNDEFINED};
};

class webgpu_texture_view_resource final : public backend_texture_view_resource {
public:
  explicit webgpu_texture_view_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_texture_view_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->provider->destroy_texture_view(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_texture_view handle_{};
};

class webgpu_sampler_resource final : public backend_sampler_resource {
public:
  explicit webgpu_sampler_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_sampler_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->provider->destroy_sampler(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_sampler handle_{};
};

class webgpu_bind_group_layout_resource final : public backend_bind_group_layout_resource {
public:
  explicit webgpu_bind_group_layout_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_bind_group_layout_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->provider->destroy_bind_group_layout(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_bind_group_layout handle_{};
};

class webgpu_bind_group_resource final : public backend_bind_group_resource {
public:
  explicit webgpu_bind_group_resource(std::shared_ptr<webgpu_resource_context> context)
      : context_(std::move(context)) {}
  ~webgpu_bind_group_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->provider->destroy_bind_group(context_->instance, handle_));
  }

  std::shared_ptr<webgpu_resource_context> context_;
  granit_webgpu_provider_bind_group handle_{};
};

webgpu_buffer_resource* as_buffer(backend_buffer_resource& resource) noexcept {
  return dynamic_cast<webgpu_buffer_resource*>(&resource);
}

const webgpu_buffer_resource* as_buffer(const backend_buffer_resource& resource) noexcept {
  return dynamic_cast<const webgpu_buffer_resource*>(&resource);
}

granit_webgpu_provider_texture_format to_format(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_R8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_RG8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_RGBA8_SRGB;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_RGBA16_FLOAT;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_D32_FLOAT;
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC1_RGBA_UNORM;
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_SRGB:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC1_RGBA_SRGB;
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC3_RGBA_UNORM;
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_SRGB:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC3_RGBA_SRGB;
  case GRANIT_TEXTURE_FORMAT_BC5_RG_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC5_RG_UNORM;
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC7_RGBA_UNORM;
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_SRGB:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BC7_RGBA_SRGB;
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_ETC2_RGBA8_UNORM;
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_SRGB:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_ETC2_RGBA8_SRGB;
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_UNORM:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_ASTC_4X4_UNORM;
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_SRGB:
    return GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_ASTC_4X4_SRGB;
  default:
    return 0;
  }
}

granit_webgpu_provider_texture_usage to_usage(granit_texture_usage usage) noexcept {
  granit_webgpu_provider_texture_usage result{};
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_TEXTURE_USAGE_COPY_SRC_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_TEXTURE_USAGE_COPY_DST_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_SAMPLED_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_TEXTURE_USAGE_SAMPLED_BIT;
  if ((usage & (GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT;
  return result;
}

granit_webgpu_provider_buffer_usage to_usage(granit_buffer_usage usage,
                                             granit_memory_location location) noexcept {
  granit_webgpu_provider_buffer_usage result{};
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_COPY_SRC_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_COPY_DST_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_VERTEX_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_INDEX_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_INDEX_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_UNIFORM_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_UNIFORM_BIT;
  if ((usage & GRANIT_BUFFER_USAGE_STORAGE_BIT) != 0)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_STORAGE_BIT;
  if (location == GRANIT_MEMORY_LOCATION_READBACK)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_MAP_READ_BIT |
              GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_COPY_DST_BIT;
  if (location == GRANIT_MEMORY_LOCATION_UPLOAD)
    result |= GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_COPY_DST_BIT;
  return result;
}

} // namespace

webgpu_resource_adapter::webgpu_resource_adapter(webgpu_context& provider,
                                                 granit_webgpu_provider_instance instance)
    : context_(
          std::make_shared<webgpu_resource_context>(webgpu_resource_context{&provider, instance})) {
}

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
  const auto native_size = (desc.size + 3) & ~UINT64_C(3);
  granit_webgpu_provider_buffer_desc provider_desc{sizeof(provider_desc), 0, native_size, usage, 0};
  const auto result =
      context_->provider->create_buffer(context_->instance, &provider_desc, &buffer->handle_);
  if (result == GRANIT_SUCCESS)
    buffer->memory_location_ = desc.memory_location;
  if (result == GRANIT_SUCCESS)
    buffer->size_ = desc.size;
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
  return context_->provider->write_buffer(context_->instance, buffer->handle_, offset,
                                          buffer->host_memory_.data() + offset, size);
}

granit_result webgpu_resource_adapter::invalidate(backend_buffer_resource& resource,
                                                  std::uint64_t offset,
                                                  std::uint64_t size) const noexcept {
  auto* buffer = as_buffer(resource);
  if (buffer == nullptr || buffer->memory_location_ != GRANIT_MEMORY_LOCATION_READBACK ||
      offset > buffer->host_memory_.size() || size > buffer->host_memory_.size() - offset)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->provider->read_buffer(context_->instance, buffer->handle_, offset,
                                         buffer->host_memory_.data() + offset, size);
}

granit_result webgpu_resource_adapter::upload(backend_buffer_resource& resource,
                                              std::uint64_t offset, const void* data,
                                              std::uint64_t size) const noexcept {
  auto* buffer = as_buffer(resource);
  return buffer == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                           : context_->provider->write_buffer(context_->instance, buffer->handle_,
                                                              offset, data, size);
}

granit_result webgpu_resource_adapter::upload_batch(
    std::span<const backend_upload_operation> uploads) const noexcept {
  if (uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_webgpu_provider_upload_operation> operations;
    operations.reserve(uploads.size());
    for (const auto& upload : uploads) {
      if (upload.data == nullptr || upload.size == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      granit_webgpu_provider_upload_operation operation{};
      operation.struct_size = sizeof(operation);
      operation.data = upload.data;
      operation.size = upload.size;
      if (upload.type == backend_upload_type::buffer) {
        const auto* buffer = upload.buffer == nullptr ? nullptr : as_buffer(*upload.buffer);
        if (buffer == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        operation.type = GRANIT_WEBGPU_PROVIDER_UPLOAD_TYPE_BUFFER;
        operation.buffer = buffer->handle_;
        operation.destination_offset = upload.destination_offset;
      } else if (upload.type == backend_upload_type::texture) {
        const auto* texture = upload.texture == nullptr
                                  ? nullptr
                                  : dynamic_cast<const webgpu_texture_resource*>(upload.texture);
        if (texture == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        const auto& copy = upload.texture_copy;
        const auto block = texture_format_block(texture->format_);
        if (block.bytes == 0 || copy.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || copy.z != 0 ||
            copy.depth != 1 || copy.x < 0 || copy.y < 0 ||
            (copy.buffer_row_length != 0 && copy.buffer_row_length % block.width != 0) ||
            (copy.buffer_image_height != 0 && copy.buffer_image_height % block.height != 0) ||
            (copy.buffer_row_length != 0 &&
             copy.buffer_row_length / block.width > UINT32_MAX / block.bytes))
          return GRANIT_ERROR_UNSUPPORTED;
        operation.type = GRANIT_WEBGPU_PROVIDER_UPLOAD_TYPE_TEXTURE;
        operation.texture = texture->handle_;
        operation.texture_write = {
            sizeof(granit_webgpu_provider_texture_write_desc),
            copy.mip_level,
            static_cast<std::uint32_t>(copy.x),
            static_cast<std::uint32_t>(copy.y),
            copy.width,
            copy.height,
            copy.buffer_row_length == 0 ? 0 : copy.buffer_row_length / block.width * block.bytes,
            copy.buffer_image_height == 0 ? 0 : copy.buffer_image_height / block.height,
            copy.base_array_layer,
            copy.array_layer_count};
      } else {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      operations.push_back(operation);
    }
    return context_->provider->write_upload_batch(context_->instance, operations);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_resource_adapter::upload_batch_async(
    std::span<const backend_upload_operation> uploads,
    std::unique_ptr<backend_upload_completion>& completion) const noexcept {
  completion.reset();
  try {
    auto candidate = std::make_unique<completed_upload>();
    const auto result = upload_batch(uploads);
    if (result != GRANIT_SUCCESS)
      return result;
    completion = std::move(candidate);
    return result;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_resource_adapter::readback_batch_async(
    std::span<const backend_readback_operation> readbacks, granit_readback_layout layout,
    std::uint64_t max_result_bytes,
    std::unique_ptr<backend_readback_completion>& completion) const noexcept {
  completion.reset();
  if (readbacks.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_webgpu_provider_buffer staging{};
  granit_webgpu_provider_command_recorder recorder{};
  granit_webgpu_provider_command_buffer command{};
  granit_webgpu_provider_readback operation{};
  auto cleanup = [&] {
    if (operation != 0)
      static_cast<void>(context_->provider->destroy_readback(context_->instance, operation));
    if (command != 0)
      static_cast<void>(context_->provider->destroy_command_buffer(context_->instance, command));
    if (recorder != 0)
      static_cast<void>(context_->provider->destroy_command_recorder(context_->instance, recorder));
    if (staging != 0)
      static_cast<void>(context_->provider->destroy_buffer(context_->instance, staging));
  };
  try {
    std::vector<webgpu_readback_slice> slices;
    slices.reserve(readbacks.size());
    std::uint64_t required{};
    std::uint64_t result_bytes{};
    for (const auto& readback : readbacks) {
      required = (required + 255) & ~UINT64_C(255);
      const auto buffer_prefix = readback.source_offset & UINT64_C(3);
      webgpu_readback_slice slice{.source_offset = required,
                                  .result_size = readback.size,
                                  .source_bytes_per_row = static_cast<std::uint32_t>(readback.size),
                                  .result_bytes_per_row = static_cast<std::uint32_t>(readback.size),
                                  .rows = 1,
                                  .layers = 1,
                                  .info = readback.result_info};
      if (readback.type == backend_readback_type::texture) {
        const auto row = readback.result_info.bytes_per_row;
        const auto padded_row = (row + 255) & ~UINT32_C(255);
        slice.source_bytes_per_row = padded_row;
        slice.result_bytes_per_row = layout == GRANIT_READBACK_LAYOUT_TIGHT ? row : padded_row;
        slice.rows = readback.result_info.rows_per_image;
        slice.layers = readback.result_info.array_layer_count;
        slice.result_size =
            static_cast<std::uint64_t>(slice.result_bytes_per_row) * slice.rows * slice.layers;
        slice.info.required_size = slice.result_size;
        slice.info.bytes_per_row = slice.result_bytes_per_row;
        required += static_cast<std::uint64_t>(padded_row) * slice.rows * slice.layers;
      } else {
        slice.source_offset += buffer_prefix;
        required += (buffer_prefix + readback.size + 3) & ~UINT64_C(3);
      }
      if (result_bytes > UINT64_MAX - slice.result_size)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      result_bytes += slice.result_size;
      slices.push_back(slice);
    }
    if (max_result_bytes != 0 && result_bytes > max_result_bytes)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    granit_webgpu_provider_buffer_desc desc{sizeof(desc), 0, required,
                                            GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_MAP_READ_BIT |
                                                GRANIT_WEBGPU_PROVIDER_BUFFER_USAGE_COPY_DST_BIT,
                                            0};
    auto result = context_->provider->create_buffer(context_->instance, &desc, &staging);
    if (result != GRANIT_SUCCESS)
      return result;
    result = context_->provider->create_command_recorder(context_->instance, &recorder);
    if (result != GRANIT_SUCCESS) {
      cleanup();
      return result;
    }
    for (std::size_t index = 0; index < readbacks.size(); ++index) {
      const auto& readback = readbacks[index];
      if (readback.type == backend_readback_type::buffer) {
        const auto* source = readback.buffer == nullptr ? nullptr : as_buffer(*readback.buffer);
        if (source == nullptr) {
          cleanup();
          return GRANIT_ERROR_INVALID_ARGUMENT;
        }
        const auto source_start = readback.source_offset & ~UINT64_C(3);
        const auto prefix = readback.source_offset - source_start;
        const auto copy_size = (prefix + readback.size + 3) & ~UINT64_C(3);
        granit_webgpu_provider_buffer_copy_region region{source_start, slices[index].source_offset,
                                                         copy_size};
        result = context_->provider->recorder_copy_buffer(context_->instance, recorder,
                                                          source->handle_, staging, {&region, 1});
      } else {
        const auto* source = readback.texture == nullptr
                                 ? nullptr
                                 : dynamic_cast<const webgpu_texture_resource*>(readback.texture);
        if (source == nullptr) {
          cleanup();
          return GRANIT_ERROR_INVALID_ARGUMENT;
        }
        const auto& region = readback.texture_region;
        granit_webgpu_provider_texture_buffer_copy copy{slices[index].source_offset,
                                                        slices[index].source_bytes_per_row,
                                                        slices[index].rows,
                                                        region.mip_level,
                                                        region.base_array_layer,
                                                        region.array_layer_count,
                                                        GRANIT_WEBGPU_PROVIDER_TEXTURE_ASPECT_ALL,
                                                        region.x,
                                                        region.y,
                                                        region.z,
                                                        region.width,
                                                        region.height,
                                                        region.depth};
        result = context_->provider->recorder_copy_texture_to_buffer_v2(
            context_->instance, recorder, source->handle_, staging, copy);
      }
      if (result != GRANIT_SUCCESS) {
        cleanup();
        return result;
      }
    }
    result = context_->provider->finish_command_recorder(context_->instance, recorder, &command);
    recorder = 0;
    if (result == GRANIT_SUCCESS)
      result = context_->provider->submit_command_buffer(context_->instance, command);
    command = 0;
    if (result == GRANIT_SUCCESS)
      result =
          context_->provider->begin_readback(context_->instance, staging, 0, required, &operation);
    if (result != GRANIT_SUCCESS) {
      cleanup();
      return result;
    }
    completion = std::make_unique<webgpu_readback_completion>(context_, staging, operation,
                                                              std::move(slices));
    staging = 0;
    operation = 0;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    cleanup();
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    cleanup();
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_webgpu_provider_buffer
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
  const granit_webgpu_provider_texture_desc provider_desc{
      sizeof(provider_desc),
      0,
      desc.width,
      desc.height,
      usage,
      format,
      desc.mip_levels,
      desc.dimension == GRANIT_TEXTURE_DIMENSION_CUBE
          ? GRANIT_WEBGPU_PROVIDER_TEXTURE_DIMENSION_CUBE
          : GRANIT_WEBGPU_PROVIDER_TEXTURE_DIMENSION_2D,
      desc.array_layers,
      desc.sample_count};
  const auto result =
      context_->provider->create_texture(context_->instance, &provider_desc, &texture->handle_);
  if (result == GRANIT_SUCCESS)
    texture->format_ = desc.format;
  return result;
}

granit_webgpu_provider_texture
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
  const granit_webgpu_provider_texture_write_desc desc{
      sizeof(granit_webgpu_provider_texture_write_desc),
      region.mip_level,
      region.x,
      region.y,
      region.width,
      region.height,
      layout.bytes_per_row,
      layout.rows_per_image,
      region.base_array_layer,
      region.array_layer_count};
  return context_->provider->write_texture(context_->instance, texture->handle_, &desc, data, size);
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
  const granit_webgpu_provider_texture_view_desc provider_desc{
      sizeof(provider_desc),
      format,
      desc.range.base_mip_level,
      mip_count,
      desc.dimension == GRANIT_TEXTURE_DIMENSION_CUBE
          ? GRANIT_WEBGPU_PROVIDER_TEXTURE_DIMENSION_CUBE
          : GRANIT_WEBGPU_PROVIDER_TEXTURE_DIMENSION_2D,
      desc.range.base_array_layer,
      desc.range.array_layer_count};
  return context_->provider->create_texture_view(context_->instance, native_texture->handle_,
                                                 &provider_desc, &view->handle_);
}

granit_webgpu_provider_texture_view webgpu_resource_adapter::native_texture_view(
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
  const granit_webgpu_provider_sampler_desc provider_desc{
      sizeof(granit_webgpu_provider_sampler_desc),
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
  return context_->provider->create_sampler(context_->instance, &provider_desc, &sampler->handle_);
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
  std::vector<granit_webgpu_provider_bind_group_layout_entry> provider_entries;
  try {
    provider_entries.reserve(entries.size());
    for (const auto& entry : entries) {
      granit_webgpu_provider_binding_type type{};
      switch (entry.type) {
      case GRANIT_BINDING_TYPE_UNIFORM_BUFFER:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_UNIFORM_BUFFER;
        break;
      case GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        break;
      case GRANIT_BINDING_TYPE_STORAGE_BUFFER:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_STORAGE_BUFFER;
        break;
      case GRANIT_BINDING_TYPE_SAMPLED_TEXTURE:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLED_TEXTURE;
        break;
      case GRANIT_BINDING_TYPE_SAMPLED_TEXTURE_CUBE:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLED_TEXTURE_CUBE;
        break;
      case GRANIT_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE;
        break;
      case GRANIT_BINDING_TYPE_SAMPLER:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLER;
        break;
      case GRANIT_BINDING_TYPE_COMPARISON_SAMPLER:
        type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_COMPARISON_SAMPLER;
        break;
      default:
        return GRANIT_ERROR_UNSUPPORTED;
      }
      if (entry.array_count != 1)
        return GRANIT_ERROR_UNSUPPORTED;
      provider_entries.push_back({entry.binding, type, entry.visibility, entry.array_count});
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  const granit_webgpu_provider_bind_group_layout_desc desc{
      sizeof(granit_webgpu_provider_bind_group_layout_desc),
      static_cast<std::uint32_t>(provider_entries.size()), provider_entries.data(), 0};
  return context_->provider->create_bind_group_layout(context_->instance, &desc, &layout->handle_);
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
  std::vector<granit_webgpu_provider_bind_group_entry> entries;
  try {
    entries.reserve(writes.size());
    for (const auto& write : writes) {
      granit_webgpu_provider_bind_group_entry entry{};
      entry.binding = write.binding;
      entry.offset = write.offset;
      entry.size = write.range;
      switch (write.type) {
      case backend_binding_type::uniform_buffer:
        entry.type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_UNIFORM_BUFFER;
        entry.buffer = native_buffer(*write.buffer);
        break;
      case backend_binding_type::dynamic_uniform_buffer:
        entry.type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        entry.buffer = native_buffer(*write.buffer);
        break;
      case backend_binding_type::storage_buffer:
        entry.type = GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_STORAGE_BUFFER;
        entry.buffer = native_buffer(*write.buffer);
        break;
      case backend_binding_type::sampled_texture:
      case backend_binding_type::sampled_texture_cube:
      case backend_binding_type::sampled_depth_texture: {
        const auto* view = dynamic_cast<webgpu_texture_view_resource*>(write.texture_view);
        if (view == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.type = write.type == backend_binding_type::sampled_texture_cube
                         ? GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLED_TEXTURE_CUBE
                     : write.type == backend_binding_type::sampled_depth_texture
                         ? GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE
                         : GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLED_TEXTURE;
        entry.texture_view = view->handle_;
        break;
      }
      case backend_binding_type::sampler:
      case backend_binding_type::comparison_sampler: {
        const auto* sampler = dynamic_cast<webgpu_sampler_resource*>(write.sampler);
        if (sampler == nullptr)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.type = write.type == backend_binding_type::comparison_sampler
                         ? GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_COMPARISON_SAMPLER
                         : GRANIT_WEBGPU_PROVIDER_BINDING_TYPE_SAMPLER;
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
  const granit_webgpu_provider_bind_group_desc desc{sizeof(granit_webgpu_provider_bind_group_desc),
                                                    static_cast<std::uint32_t>(entries.size()),
                                                    native_layout->handle_, entries.data(), 0};
  return context_->provider->create_bind_group(context_->instance, &desc, &group->handle_);
}

granit_webgpu_provider_bind_group_layout webgpu_resource_adapter::native_bind_group_layout(
    backend_bind_group_layout_resource& resource) const noexcept {
  const auto* layout = dynamic_cast<webgpu_bind_group_layout_resource*>(&resource);
  return layout == nullptr ? 0 : layout->handle_;
}

granit_webgpu_provider_bind_group
webgpu_resource_adapter::native_bind_group(backend_bind_group_resource& resource) const noexcept {
  const auto* group = dynamic_cast<webgpu_bind_group_resource*>(&resource);
  return group == nullptr ? 0 : group->handle_;
}

} // namespace granit::detail
