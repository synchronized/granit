// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/resource_adapter.h"

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

webgpu_buffer_resource* as_buffer(backend_buffer_resource& resource) noexcept {
  return dynamic_cast<webgpu_buffer_resource*>(&resource);
}

const webgpu_buffer_resource* as_buffer(const backend_buffer_resource& resource) noexcept {
  return dynamic_cast<const webgpu_buffer_resource*>(&resource);
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
  for (const auto& upload : uploads) {
    if (upload.type != backend_upload_type::buffer || upload.buffer == nullptr)
      return GRANIT_ERROR_UNSUPPORTED;
    const auto* buffer = as_buffer(*upload.buffer);
    if (buffer == nullptr)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto result = context_->loader->write_buffer(
        context_->instance, buffer->handle_, upload.destination_offset, upload.data, upload.size);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  return GRANIT_SUCCESS;
}

granit_backend_plugin_buffer
webgpu_resource_adapter::native_buffer(backend_buffer_resource& resource) const noexcept {
  const auto* buffer = as_buffer(resource);
  return buffer == nullptr ? 0 : buffer->handle_;
}

} // namespace granit::detail
