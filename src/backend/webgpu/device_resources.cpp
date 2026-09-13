// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/webgpu/device_state.h"
#include "backend/webgpu/device_utils.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

constexpr std::uint64_t request_timeout_ns = UINT64_C(10000000000);

struct map_request {
  const webgpu_host_api* host{};
  WGPUMapAsyncStatus status{};
};

struct readback_map_request {
  std::shared_ptr<webgpu_device_state::readback_record> readback;
};

void emit_dawn_message(const webgpu_host_api* host, WGPUStringView message) noexcept {
  if (host == nullptr || host->diagnostic_callback == nullptr || message.data == nullptr)
    return;
  const auto length = message.length == WGPU_STRLEN ? std::strlen(message.data) : message.length;
  const auto bounded_length = static_cast<std::uint32_t>(
      (std::min)(length, static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())));
  try {
    host->diagnostic_callback(GRANIT_DIAGNOSTIC_SEVERITY_ERROR, GRANIT_DIAGNOSTIC_CATEGORY_DEVICE,
                              message.data, bounded_length, host->diagnostic_user_data);
  } catch (...) {
  }
}

void receive_map(WGPUMapAsyncStatus status, WGPUStringView message, void* data, void*) noexcept {
  auto& request = *static_cast<map_request*>(data);
  request.status = status;
  if (status != WGPUMapAsyncStatus_Success)
    emit_dawn_message(request.host, message);
}

void receive_readback_map(WGPUMapAsyncStatus status, WGPUStringView, void* data, void*) noexcept {
  std::unique_ptr<readback_map_request> request{static_cast<readback_map_request*>(data)};
  auto& readback = *request->readback;
  if (status != WGPUMapAsyncStatus_Success) {
    readback.state.store(3, std::memory_order_release);
    return;
  }
  const auto* mapped =
      wgpuBufferGetConstMappedRange(readback.buffer, static_cast<std::size_t>(readback.offset),
                                    static_cast<std::size_t>(readback.size));
  if (mapped == nullptr) {
    readback.state.store(3, std::memory_order_release);
    return;
  }
  std::memcpy(readback.bytes.data(), mapped, static_cast<std::size_t>(readback.size));
  wgpuBufferUnmap(readback.buffer);
  readback.state.store(2, std::memory_order_release);
}

granit_result create_buffer(webgpu_instance_handle instance, const webgpu_buffer_desc* desc,
                            webgpu_buffer* out_buffer) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(webgpu_buffer_desc, reserved_flags) + sizeof(std::uint32_t);
  constexpr auto known_usage =
      GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT | GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT |
      GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT | GRANIT_WEBGPU_BUFFER_USAGE_VERTEX_BIT |
      GRANIT_WEBGPU_BUFFER_USAGE_INDEX_BIT | GRANIT_WEBGPU_BUFFER_USAGE_UNIFORM_BIT |
      GRANIT_WEBGPU_BUFFER_USAGE_STORAGE_BIT;
  if (out_buffer != nullptr) {
    *out_buffer = 0;
  }
  if (instance == 0 || desc == nullptr || out_buffer == nullptr ||
      desc->struct_size < minimum_size || desc->reserved != 0 || desc->reserved_flags != 0 ||
      desc->size == 0 || desc->usage == 0 || (desc->usage & ~known_usage) != 0 ||
      ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT) != 0 &&
       (desc->usage & ~GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT &
        ~GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) != 0)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  if (desc->size > state.capabilities.max_buffer_size) {
    return GRANIT_ERROR_UNSUPPORTED;
  }

  WGPUBufferUsage usage = WGPUBufferUsage_None;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT) != 0)
    usage |= WGPUBufferUsage_MapRead;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT) != 0)
    usage |= WGPUBufferUsage_CopySrc;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) != 0)
    usage |= WGPUBufferUsage_CopyDst;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_VERTEX_BIT) != 0)
    usage |= WGPUBufferUsage_Vertex;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_INDEX_BIT) != 0)
    usage |= WGPUBufferUsage_Index;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_UNIFORM_BIT) != 0)
    usage |= WGPUBufferUsage_Uniform;
  if ((desc->usage & GRANIT_WEBGPU_BUFFER_USAGE_STORAGE_BIT) != 0)
    usage |= WGPUBufferUsage_Storage;
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.usage = usage;
  descriptor.size = desc->size;
  const auto native = wgpuDeviceCreateBuffer(state.device, &descriptor);
  if (native == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  auto handle = next_buffer.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0) {
    handle = next_buffer.fetch_add(1, std::memory_order_relaxed);
  }
  try {
    const auto [iterator, inserted] = state.buffers.emplace(
        handle, webgpu_device_state::buffer_record{native, desc->size, desc->usage});
    static_cast<void>(iterator);
    if (!inserted) {
      wgpuBufferRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuBufferRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuBufferRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_buffer = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_buffer(webgpu_instance_handle instance, webgpu_buffer buffer) noexcept {
  if (instance == 0 || buffer == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto buffer_found = found->second->buffers.find(buffer);
  if (buffer_found == found->second->buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (std::any_of(found->second->bind_groups.begin(), found->second->bind_groups.end(),
                  [buffer](const auto& entry) {
                    return std::find(entry.second.buffers.begin(), entry.second.buffers.end(),
                                     buffer) != entry.second.buffers.end();
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuBufferRelease(buffer_found->second.buffer);
  found->second->buffers.erase(buffer_found);
  return GRANIT_SUCCESS;
}

granit_result write_buffer(webgpu_instance_handle instance, webgpu_buffer buffer,
                           std::uint64_t offset, const void* data, std::uint64_t size) noexcept {
  if (instance == 0 || buffer == 0 || data == nullptr || size == 0 || offset % 4 != 0 ||
      size % 4 != 0 || size > static_cast<std::uint64_t>(SIZE_MAX)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto buffer_found = found->second->buffers.find(buffer);
  if (buffer_found == found->second->buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& record = buffer_found->second;
  if ((record.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0 || offset > record.size ||
      size > record.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  wgpuQueueWriteBuffer(found->second->queue, record.buffer, offset, data,
                       static_cast<std::size_t>(size));
  return GRANIT_SUCCESS;
}

granit_result read_buffer(webgpu_instance_handle instance, webgpu_buffer buffer,
                          std::uint64_t offset, void* data, std::uint64_t size) noexcept {
  if (instance == 0 || buffer == 0 || data == nullptr || size == 0 || offset % 8 != 0 ||
      size % 4 != 0 || size > static_cast<std::uint64_t>(SIZE_MAX)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto buffer_found = found->second->buffers.find(buffer);
  if (buffer_found == found->second->buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& record = buffer_found->second;
  if ((record.usage & GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT) == 0 || offset > record.size ||
      size > record.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  map_request request{&found->second->host};
  const WGPUBufferMapCallbackInfo callback{nullptr, WGPUCallbackMode_WaitAnyOnly, receive_map,
                                           &request, nullptr};
  const auto future =
      wgpuBufferMapAsync(record.buffer, WGPUMapMode_Read, static_cast<std::size_t>(offset),
                         static_cast<std::size_t>(size), callback);
  WGPUFutureWaitInfo wait_info{future, WGPU_FALSE};
  const auto wait_status =
      wgpuInstanceWaitAny(found->second->instance, 1, &wait_info, request_timeout_ns);
  if (wait_status != WGPUWaitStatus_Success || !wait_info.completed) {
    wgpuBufferUnmap(record.buffer);
    return GRANIT_ERROR_NOT_READY;
  }
  if (request.status != WGPUMapAsyncStatus_Success) {
    wgpuBufferUnmap(record.buffer);
    return GRANIT_ERROR_INTERNAL;
  }
  const auto* mapped = wgpuBufferGetConstMappedRange(
      record.buffer, static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
  if (mapped == nullptr) {
    wgpuBufferUnmap(record.buffer);
    return GRANIT_ERROR_INTERNAL;
  }
  std::memcpy(data, mapped, static_cast<std::size_t>(size));
  wgpuBufferUnmap(record.buffer);
  return GRANIT_SUCCESS;
}

granit_result destroy_readback(webgpu_instance_handle instance, webgpu_readback readback) noexcept;

granit_result begin_readback(webgpu_instance_handle instance, webgpu_buffer buffer,
                             std::uint64_t offset, std::uint64_t size,
                             webgpu_readback* readback) noexcept {
  if (instance == 0 || buffer == 0 || readback == nullptr || size == 0 || offset % 8 != 0 ||
      size % 4 != 0 || size > static_cast<std::uint64_t>(SIZE_MAX))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::shared_ptr<webgpu_device_state::readback_record> record;
  {
    const std::scoped_lock lock{instances_mutex};
    const auto found = instances.find(instance);
    if (found == instances.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
      return ready;
    const auto buffer_found = found->second->buffers.find(buffer);
    if (buffer_found == found->second->buffers.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& source = buffer_found->second;
    if ((source.usage & GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT) == 0 || offset > source.size ||
        size > source.size - offset)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    try {
      record = std::make_shared<webgpu_device_state::readback_record>();
      record->bytes.resize(static_cast<std::size_t>(size));
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
    record->buffer = source.buffer;
    wgpuBufferAddRef(record->buffer);
    record->offset = offset;
    record->size = size;
    const auto handle = next_handle<webgpu_readback>(next_readback);
    found->second->readbacks.emplace(handle, record);
    *readback = handle;
  }
  auto* request = new (std::nothrow) readback_map_request{record};
  if (request == nullptr) {
    static_cast<void>(destroy_readback(instance, *readback));
    *readback = 0;
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
#if defined(__EMSCRIPTEN__)
  constexpr auto callback_mode = WGPUCallbackMode_AllowSpontaneous;
#else
  constexpr auto callback_mode = WGPUCallbackMode_AllowProcessEvents;
#endif
  const WGPUBufferMapCallbackInfo callback{nullptr, callback_mode, receive_readback_map, request,
                                           nullptr};
  record->state.store(1, std::memory_order_release);
  static_cast<void>(wgpuBufferMapAsync(record->buffer, WGPUMapMode_Read,
                                       static_cast<std::size_t>(offset),
                                       static_cast<std::size_t>(size), callback));
  return GRANIT_SUCCESS;
}

granit_result poll_readback(webgpu_instance_handle instance, webgpu_readback readback) noexcept {
  std::shared_ptr<webgpu_device_state::readback_record> record;
  WGPUInstance native{};
  {
    const std::scoped_lock lock{instances_mutex};
    const auto found = instances.find(instance);
    if (found == instances.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto operation = found->second->readbacks.find(readback);
    if (operation == found->second->readbacks.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record = operation->second;
    native = found->second->instance;
  }
  wgpuInstanceProcessEvents(native);
  const auto state = record->state.load(std::memory_order_acquire);
  return state == 2 ? GRANIT_SUCCESS : state == 3 ? GRANIT_ERROR_INTERNAL : GRANIT_ERROR_NOT_READY;
}

granit_result copy_readback(webgpu_instance_handle instance, webgpu_readback readback,
                            std::uint64_t offset, void* data, std::uint64_t size) noexcept {
  if (data == nullptr || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto operation = found->second->readbacks.find(readback);
  if (operation == found->second->readbacks.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& record = *operation->second;
  if (record.state.load(std::memory_order_acquire) != 2)
    return GRANIT_ERROR_NOT_READY;
  if (offset > record.size || size > record.size - offset)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::memcpy(data, record.bytes.data() + offset, static_cast<std::size_t>(size));
  return GRANIT_SUCCESS;
}

granit_result destroy_readback(webgpu_instance_handle instance, webgpu_readback readback) noexcept {
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  return found->second->readbacks.erase(readback) == 1 ? GRANIT_SUCCESS
                                                       : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result create_texture(webgpu_instance_handle instance, const webgpu_texture_desc* desc,
                             webgpu_texture* out_texture) noexcept {
  constexpr auto known_usage =
      GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT | GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT |
      GRANIT_WEBGPU_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT;
  if (out_texture != nullptr) {
    *out_texture = 0;
  }
  const auto dimension = desc != nullptr && desc->dimension != 0
                             ? desc->dimension
                             : GRANIT_WEBGPU_TEXTURE_DIMENSION_2D;
  const auto array_layer_count =
      desc != nullptr && desc->array_layer_count != 0 ? desc->array_layer_count : 1;
  const auto sample_count = desc != nullptr && desc->sample_count != 0 ? desc->sample_count : 1;
  if (instance == 0 || desc == nullptr || out_texture == nullptr ||
      desc->struct_size < sizeof(webgpu_texture_desc) || desc->reserved != 0 || desc->width == 0 ||
      desc->height == 0 || desc->usage == 0 || desc->mip_level_count == 0 ||
      (dimension != GRANIT_WEBGPU_TEXTURE_DIMENSION_2D &&
       dimension != GRANIT_WEBGPU_TEXTURE_DIMENSION_CUBE) ||
      (dimension == GRANIT_WEBGPU_TEXTURE_DIMENSION_CUBE &&
       (desc->width != desc->height || array_layer_count != 6)) ||
      (sample_count != 1 && sample_count != 4) ||
      (sample_count > 1 &&
       (desc->mip_level_count != 1 || array_layer_count != 1 ||
        dimension != GRANIT_WEBGPU_TEXTURE_DIMENSION_2D ||
        (desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) == 0)) ||
      to_native_texture_format(desc->format) == WGPUTextureFormat_Undefined ||
      (desc->usage & ~known_usage) != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  if (desc->width > state.capabilities.max_texture_dimension_2d ||
      desc->height > state.capabilities.max_texture_dimension_2d) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  WGPUTextureUsage usage = WGPUTextureUsage_None;
  if ((desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) != 0)
    usage |= WGPUTextureUsage_CopySrc;
  if ((desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) != 0)
    usage |= WGPUTextureUsage_CopyDst;
  if ((desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_SAMPLED_BIT) != 0)
    usage |= WGPUTextureUsage_TextureBinding;
  if ((desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) != 0)
    usage |= WGPUTextureUsage_RenderAttachment;
  if (desc->mip_level_count > 1 && sample_count == 1 &&
      (desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) != 0 &&
      (desc->usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) != 0 &&
      desc->format != GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT &&
      texture_block(desc->format).width == 1) {
    // 公共契约用 Transfer usage 表达 Mipmap；内部补充渲染采样 usage，避免泄漏 WebGPU 实现路径。
    usage |= WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment;
  }
  WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
  descriptor.usage = usage;
  descriptor.dimension = WGPUTextureDimension_2D;
  descriptor.size = {desc->width, desc->height, array_layer_count};
  descriptor.format = to_native_texture_format(desc->format);
  descriptor.mipLevelCount = desc->mip_level_count;
  descriptor.sampleCount = sample_count;
  const auto native = wgpuDeviceCreateTexture(state.device, &descriptor);
  if (native == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  const auto handle = next_handle<webgpu_texture>(next_texture);
  try {
    if (!state.textures
             .emplace(handle,
                      webgpu_device_state::texture_record{
                          native, desc->width, desc->height, desc->format, desc->mip_level_count,
                          array_layer_count, sample_count, desc->usage, false})
             .second) {
      wgpuTextureRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuTextureRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuTextureRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_texture = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_texture(webgpu_instance_handle instance, webgpu_texture texture) noexcept {
  if (instance == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto texture_found = state.textures.find(texture);
  if (texture_found == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (texture_found->second.borrowed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::any_of(state.texture_views.begin(), state.texture_views.end(),
                  [texture](const auto& entry) { return entry.second.texture == texture; })) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  wgpuTextureRelease(texture_found->second.texture);
  state.textures.erase(texture_found);
  return GRANIT_SUCCESS;
}

granit_result write_texture(webgpu_instance_handle instance, webgpu_texture texture,
                            const webgpu_texture_write_desc* desc, const void* data,
                            std::uint64_t size) noexcept {
  const auto array_layer_count =
      desc != nullptr && desc->array_layer_count != 0 ? desc->array_layer_count : 1;
  if (instance == 0 || texture == 0 || desc == nullptr || data == nullptr || size == 0 ||
      desc->struct_size < sizeof(webgpu_texture_write_desc) || desc->width == 0 ||
      desc->height == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto texture_found = found->second->textures.find(texture);
  if (texture_found == found->second->textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& record = texture_found->second;
  if (record.borrowed || (record.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->mip_level >= record.mip_level_count ||
      desc->base_array_layer >= record.array_layer_count ||
      array_layer_count > record.array_layer_count - desc->base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto mip_width = std::max(UINT32_C(1), record.width >> desc->mip_level);
  const auto mip_height = std::max(UINT32_C(1), record.height >> desc->mip_level);
  if (desc->x >= mip_width || desc->width > mip_width - desc->x || desc->y >= mip_height ||
      desc->height > mip_height - desc->y)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto block = texture_block(record.format);
  if (!valid_texture_block_region(record.format, desc->x, desc->y, desc->width, desc->height,
                                  mip_width, mip_height))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t block_rows = (std::uint64_t{desc->height} + block.height - 1) / block.height;
  const std::uint64_t tight_row =
      ((std::uint64_t{desc->width} + block.width - 1) / block.width) * block.bytes;
  const std::uint64_t row_pitch = desc->bytes_per_row == 0 ? tight_row : desc->bytes_per_row;
  const std::uint64_t rows = desc->rows_per_image == 0 ? block_rows : desc->rows_per_image;
  if (row_pitch < tight_row || row_pitch % block.bytes != 0 || rows < block_rows ||
      row_pitch > std::numeric_limits<std::uint32_t>::max() ||
      rows > std::numeric_limits<std::uint32_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_pitch = rows * row_pitch;
  const std::uint64_t required = (std::uint64_t{array_layer_count} - 1) * image_pitch +
                                 (block_rows - 1) * row_pitch + tight_row;
  if (required > size || size > std::numeric_limits<std::size_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  destination.texture = record.texture;
  destination.mipLevel = desc->mip_level;
  destination.origin = {desc->x, desc->y, desc->base_array_layer};
  destination.aspect = WGPUTextureAspect_All;
  WGPUTexelCopyBufferLayout layout{};
  layout.offset = 0;
  layout.bytesPerRow = static_cast<std::uint32_t>(row_pitch);
  layout.rowsPerImage = static_cast<std::uint32_t>(rows);
  const WGPUExtent3D extent{desc->width, desc->height, array_layer_count};
  wgpuQueueWriteTexture(found->second->queue, &destination, data, static_cast<std::size_t>(size),
                        &layout, &extent);
  return GRANIT_SUCCESS;
}

granit_result write_upload_batch(webgpu_instance_handle instance,
                                 const webgpu_upload_operation* operations,
                                 std::uint32_t operation_count) noexcept {
  if (instance == 0 || operations == nullptr || operation_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;

  for (std::uint32_t index = 0; index < operation_count; ++index) {
    const auto& operation = operations[index];
    if (operation.struct_size < sizeof(webgpu_upload_operation) || operation.data == nullptr ||
        operation.size == 0 || operation.reserved != 0 ||
        operation.size > static_cast<std::uint64_t>(SIZE_MAX))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (operation.type == GRANIT_WEBGPU_UPLOAD_TYPE_BUFFER) {
      if (operation.buffer == 0 || operation.texture != 0 ||
          operation.texture_write.struct_size != 0 || operation.destination_offset % 4 != 0 ||
          operation.size % 4 != 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto buffer = state.buffers.find(operation.buffer);
      if (buffer == state.buffers.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0 ||
          operation.destination_offset > buffer->second.size ||
          operation.size > buffer->second.size - operation.destination_offset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      continue;
    }
    if (operation.type != GRANIT_WEBGPU_UPLOAD_TYPE_TEXTURE || operation.buffer != 0 ||
        operation.texture == 0 || operation.destination_offset != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto& desc = operation.texture_write;
    if (desc.struct_size < sizeof(webgpu_texture_write_desc) || desc.width == 0 || desc.height == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto texture = state.textures.find(operation.texture);
    if (texture == state.textures.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& record = texture->second;
    const auto array_layer_count = desc.array_layer_count == 0 ? 1 : desc.array_layer_count;
    if (record.borrowed || (record.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
        desc.mip_level >= record.mip_level_count ||
        desc.base_array_layer >= record.array_layer_count ||
        array_layer_count > record.array_layer_count - desc.base_array_layer)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto mip_width = std::max(UINT32_C(1), record.width >> desc.mip_level);
    const auto mip_height = std::max(UINT32_C(1), record.height >> desc.mip_level);
    if (desc.x >= mip_width || desc.width > mip_width - desc.x || desc.y >= mip_height ||
        desc.height > mip_height - desc.y)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto block = texture_block(record.format);
    if (!valid_texture_block_region(record.format, desc.x, desc.y, desc.width, desc.height,
                                    mip_width, mip_height))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const std::uint64_t block_rows = (std::uint64_t{desc.height} + block.height - 1) / block.height;
    const std::uint64_t tight_row =
        ((std::uint64_t{desc.width} + block.width - 1) / block.width) * block.bytes;
    const std::uint64_t row_pitch = desc.bytes_per_row == 0 ? tight_row : desc.bytes_per_row;
    const std::uint64_t rows = desc.rows_per_image == 0 ? block_rows : desc.rows_per_image;
    if (row_pitch < tight_row || row_pitch % block.bytes != 0 || rows < block_rows ||
        row_pitch > std::numeric_limits<std::uint32_t>::max() ||
        rows > std::numeric_limits<std::uint32_t>::max())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const std::uint64_t image_pitch = rows * row_pitch;
    const std::uint64_t required = (std::uint64_t{array_layer_count} - 1) * image_pitch +
                                   (block_rows - 1) * row_pitch + tight_row;
    if (required > operation.size)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  for (std::uint32_t index = 0; index < operation_count; ++index) {
    const auto& operation = operations[index];
    if (operation.type == GRANIT_WEBGPU_UPLOAD_TYPE_BUFFER) {
      const auto& buffer = state.buffers.find(operation.buffer)->second;
      wgpuQueueWriteBuffer(state.queue, buffer.buffer, operation.destination_offset, operation.data,
                           static_cast<std::size_t>(operation.size));
      continue;
    }
    const auto& desc = operation.texture_write;
    const auto& texture = state.textures.find(operation.texture)->second;
    const auto block = texture_block(texture.format);
    const std::uint64_t tight_row =
        ((std::uint64_t{desc.width} + block.width - 1) / block.width) * block.bytes;
    const auto row_pitch =
        static_cast<std::uint32_t>(desc.bytes_per_row == 0 ? tight_row : desc.bytes_per_row);
    const auto rows = desc.rows_per_image == 0 ? (desc.height + block.height - 1) / block.height
                                               : desc.rows_per_image;
    const auto array_layer_count = desc.array_layer_count == 0 ? 1 : desc.array_layer_count;
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture.texture;
    destination.mipLevel = desc.mip_level;
    destination.origin = {desc.x, desc.y, desc.base_array_layer};
    destination.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout layout{};
    layout.bytesPerRow = row_pitch;
    layout.rowsPerImage = rows;
    const WGPUExtent3D extent{desc.width, desc.height, array_layer_count};
    wgpuQueueWriteTexture(state.queue, &destination, operation.data,
                          static_cast<std::size_t>(operation.size), &layout, &extent);
  }
  return GRANIT_SUCCESS;
}

granit_result create_texture_view(webgpu_instance_handle instance, webgpu_texture texture,
                                  const webgpu_texture_view_desc* desc,
                                  webgpu_texture_view* out_view) noexcept {
  if (out_view != nullptr)
    *out_view = 0;
  const auto dimension = desc != nullptr && desc->dimension != 0
                             ? desc->dimension
                             : GRANIT_WEBGPU_TEXTURE_DIMENSION_2D;
  const auto array_layer_count =
      desc != nullptr && desc->array_layer_count != 0 ? desc->array_layer_count : 1;
  if (instance == 0 || texture == 0 || desc == nullptr || out_view == nullptr ||
      desc->struct_size < sizeof(webgpu_texture_view_desc) || desc->mip_level_count == 0 ||
      (dimension != GRANIT_WEBGPU_TEXTURE_DIMENSION_2D &&
       dimension != GRANIT_WEBGPU_TEXTURE_DIMENSION_CUBE))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto texture_found = state.textures.find(texture);
  if (texture_found == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->format != texture_found->second.format ||
      desc->base_mip_level >= texture_found->second.mip_level_count ||
      desc->mip_level_count > texture_found->second.mip_level_count - desc->base_mip_level ||
      desc->base_array_layer >= texture_found->second.array_layer_count ||
      array_layer_count > texture_found->second.array_layer_count - desc->base_array_layer ||
      (dimension == GRANIT_WEBGPU_TEXTURE_DIMENSION_CUBE && array_layer_count != 6))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  descriptor.format = to_native_texture_format(desc->format);
  descriptor.dimension = dimension == GRANIT_WEBGPU_TEXTURE_DIMENSION_CUBE
                             ? WGPUTextureViewDimension_Cube
                             : WGPUTextureViewDimension_2D;
  descriptor.baseMipLevel = desc->base_mip_level;
  descriptor.mipLevelCount = desc->mip_level_count;
  descriptor.baseArrayLayer = desc->base_array_layer;
  descriptor.arrayLayerCount = array_layer_count;
  descriptor.aspect = WGPUTextureAspect_All;
  const auto native = wgpuTextureCreateView(texture_found->second.texture, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<webgpu_texture_view>(next_texture_view);
  try {
    if (!state.texture_views
             .emplace(handle, webgpu_device_state::texture_view_record{native, texture, false})
             .second) {
      wgpuTextureViewRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuTextureViewRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuTextureViewRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_view = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_texture_view(webgpu_instance_handle instance,
                                   webgpu_texture_view view) noexcept {
  if (instance == 0 || view == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto view_found = found->second->texture_views.find(view);
  if (view_found == found->second->texture_views.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (view_found->second.borrowed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::any_of(found->second->bind_groups.begin(), found->second->bind_groups.end(),
                  [view](const auto& entry) {
                    return std::find(entry.second.texture_views.begin(),
                                     entry.second.texture_views.end(),
                                     view) != entry.second.texture_views.end();
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuTextureViewRelease(view_found->second.view);
  found->second->texture_views.erase(view_found);
  return GRANIT_SUCCESS;
}

granit_result create_sampler(webgpu_instance_handle instance, const webgpu_sampler_desc* desc,
                             webgpu_sampler* out_sampler) noexcept {
  if (out_sampler != nullptr)
    *out_sampler = 0;
  const auto valid_filter = [](webgpu_filter filter) {
    return filter == GRANIT_WEBGPU_FILTER_NEAREST || filter == GRANIT_WEBGPU_FILTER_LINEAR;
  };
  const auto valid_address_mode = [](webgpu_address_mode mode) {
    return mode >= GRANIT_WEBGPU_ADDRESS_MODE_REPEAT &&
           mode <= GRANIT_WEBGPU_ADDRESS_MODE_CLAMP_TO_EDGE;
  };
  if (instance == 0 || desc == nullptr || out_sampler == nullptr ||
      desc->struct_size < sizeof(webgpu_sampler_desc) || desc->reserved != 0 ||
      desc->reserved_2[0] != 0 || desc->reserved_2[1] != 0 || !valid_filter(desc->min_filter) ||
      !valid_filter(desc->mag_filter) || !valid_filter(desc->mipmap_filter) ||
      !valid_address_mode(desc->address_mode_u) || !valid_address_mode(desc->address_mode_v) ||
      !valid_address_mode(desc->address_mode_w) ||
      desc->compare_operation > GRANIT_WEBGPU_COMPARE_OPERATION_ALWAYS ||
      desc->max_anisotropy == 0 || desc->max_anisotropy > UINT16_MAX ||
      (desc->max_anisotropy > 1 && (desc->min_filter != GRANIT_WEBGPU_FILTER_LINEAR ||
                                    desc->mag_filter != GRANIT_WEBGPU_FILTER_LINEAR ||
                                    desc->mipmap_filter != GRANIT_WEBGPU_FILTER_LINEAR)) ||
      !std::isfinite(desc->min_lod) || !std::isfinite(desc->max_lod) || desc->min_lod < 0.0F ||
      desc->max_lod < desc->min_lod) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto to_address_mode = [](webgpu_address_mode mode) {
    switch (mode) {
    case GRANIT_WEBGPU_ADDRESS_MODE_REPEAT:
      return WGPUAddressMode_Repeat;
    case GRANIT_WEBGPU_ADDRESS_MODE_MIRROR_REPEAT:
      return WGPUAddressMode_MirrorRepeat;
    default:
      return WGPUAddressMode_ClampToEdge;
    }
  };
  const auto to_compare = [](webgpu_compare_operation operation) {
    switch (operation) {
    case GRANIT_WEBGPU_COMPARE_OPERATION_NEVER:
      return WGPUCompareFunction_Never;
    case GRANIT_WEBGPU_COMPARE_OPERATION_LESS:
      return WGPUCompareFunction_Less;
    case GRANIT_WEBGPU_COMPARE_OPERATION_EQUAL:
      return WGPUCompareFunction_Equal;
    case GRANIT_WEBGPU_COMPARE_OPERATION_LESS_EQUAL:
      return WGPUCompareFunction_LessEqual;
    case GRANIT_WEBGPU_COMPARE_OPERATION_GREATER:
      return WGPUCompareFunction_Greater;
    case GRANIT_WEBGPU_COMPARE_OPERATION_NOT_EQUAL:
      return WGPUCompareFunction_NotEqual;
    case GRANIT_WEBGPU_COMPARE_OPERATION_GREATER_EQUAL:
      return WGPUCompareFunction_GreaterEqual;
    case GRANIT_WEBGPU_COMPARE_OPERATION_ALWAYS:
      return WGPUCompareFunction_Always;
    default:
      return WGPUCompareFunction_Undefined;
    }
  };
  WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
  descriptor.addressModeU = to_address_mode(desc->address_mode_u);
  descriptor.addressModeV = to_address_mode(desc->address_mode_v);
  descriptor.addressModeW = to_address_mode(desc->address_mode_w);
  descriptor.minFilter = desc->min_filter == GRANIT_WEBGPU_FILTER_LINEAR ? WGPUFilterMode_Linear
                                                                         : WGPUFilterMode_Nearest;
  descriptor.magFilter = desc->mag_filter == GRANIT_WEBGPU_FILTER_LINEAR ? WGPUFilterMode_Linear
                                                                         : WGPUFilterMode_Nearest;
  descriptor.mipmapFilter = desc->mipmap_filter == GRANIT_WEBGPU_FILTER_LINEAR
                                ? WGPUMipmapFilterMode_Linear
                                : WGPUMipmapFilterMode_Nearest;
  descriptor.lodMinClamp = desc->min_lod;
  descriptor.lodMaxClamp = desc->max_lod;
  descriptor.compare = to_compare(desc->compare_operation);
  descriptor.maxAnisotropy = static_cast<std::uint16_t>(desc->max_anisotropy);
  const auto native = wgpuDeviceCreateSampler(found->second->device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<webgpu_sampler>(next_sampler);
  try {
    if (!found->second->samplers.emplace(handle, native).second) {
      wgpuSamplerRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuSamplerRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuSamplerRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_sampler = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_sampler(webgpu_instance_handle instance, webgpu_sampler sampler) noexcept {
  if (instance == 0 || sampler == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto sampler_found = found->second->samplers.find(sampler);
  if (sampler_found == found->second->samplers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (std::any_of(found->second->bind_groups.begin(), found->second->bind_groups.end(),
                  [sampler](const auto& entry) {
                    return std::find(entry.second.samplers.begin(), entry.second.samplers.end(),
                                     sampler) != entry.second.samplers.end();
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSamplerRelease(sampler_found->second);
  found->second->samplers.erase(sampler_found);
  return GRANIT_SUCCESS;
}

granit_result create_bind_group_layout(webgpu_instance_handle instance,
                                       const webgpu_bind_group_layout_desc* desc,
                                       webgpu_bind_group_layout* out_layout) noexcept {
  if (out_layout != nullptr)
    *out_layout = 0;
  if (instance == 0 || desc == nullptr || out_layout == nullptr ||
      desc->struct_size < sizeof(webgpu_bind_group_layout_desc) || desc->reserved != 0 ||
      (desc->entry_count != 0 && desc->entries == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  try {
    std::vector<WGPUBindGroupLayoutEntry> entries(desc->entry_count);
    std::vector<webgpu_bind_group_layout_entry> declarations;
    if (desc->entry_count != 0)
      declarations.assign(desc->entries, desc->entries + desc->entry_count);
    for (std::uint32_t index = 0; index < desc->entry_count; ++index) {
      const auto& source = desc->entries[index];
      if (source.array_count != 1 || source.visibility == 0 ||
          (source.visibility & ~UINT32_C(7)) != 0 ||
          std::any_of(desc->entries, desc->entries + index,
                      [&](const auto& previous) { return previous.binding == source.binding; }))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      auto& entry = entries[index];
      entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
      entry.binding = source.binding;
      entry.visibility = source.visibility;
      switch (source.type) {
      case GRANIT_WEBGPU_BINDING_TYPE_UNIFORM_BUFFER:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        entry.buffer.hasDynamicOffset = WGPU_TRUE;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_STORAGE_BUFFER:
        entry.buffer.type = WGPUBufferBindingType_Storage;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_TEXTURE:
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_TEXTURE_CUBE:
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        entry.texture.viewDimension = WGPUTextureViewDimension_Cube;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE:
        entry.texture.sampleType = WGPUTextureSampleType_Depth;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_SAMPLER:
        entry.sampler.type = WGPUSamplerBindingType_Filtering;
        break;
      case GRANIT_WEBGPU_BINDING_TYPE_COMPARISON_SAMPLER:
        entry.sampler.type = WGPUSamplerBindingType_Comparison;
        break;
      default:
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    std::sort(declarations.begin(), declarations.end(),
              [](const auto& left, const auto& right) { return left.binding < right.binding; });
    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    const auto native = wgpuDeviceCreateBindGroupLayout(found->second->device, &descriptor);
    if (native == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto handle = next_handle<webgpu_bind_group_layout>(next_bind_group_layout);
    try {
      webgpu_device_state::bind_group_layout_record record{native, std::move(declarations)};
      if (!found->second->bind_group_layouts.emplace(handle, std::move(record)).second) {
        wgpuBindGroupLayoutRelease(native);
        return GRANIT_ERROR_INTERNAL;
      }
    } catch (const std::bad_alloc&) {
      wgpuBindGroupLayoutRelease(native);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      wgpuBindGroupLayoutRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
    *out_layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_bind_group_layout(webgpu_instance_handle instance,
                                        webgpu_bind_group_layout layout) noexcept {
  if (instance == 0 || layout == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto layout_found = state.bind_group_layouts.find(layout);
  if (layout_found == state.bind_group_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto used_by_group =
      std::any_of(state.bind_groups.begin(), state.bind_groups.end(),
                  [layout](const auto& entry) { return entry.second.layout == layout; });
  const auto used_by_pipeline = std::any_of(
      state.pipeline_layouts.begin(), state.pipeline_layouts.end(), [layout](const auto& entry) {
        return std::find(entry.second.bind_group_layouts.begin(),
                         entry.second.bind_group_layouts.end(),
                         layout) != entry.second.bind_group_layouts.end();
      });
  if (used_by_group || used_by_pipeline)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuBindGroupLayoutRelease(layout_found->second.bind_group_layout);
  state.bind_group_layouts.erase(layout_found);
  return GRANIT_SUCCESS;
}

granit_result create_bind_group(webgpu_instance_handle instance, const webgpu_bind_group_desc* desc,
                                webgpu_bind_group* out_bind_group) noexcept {
  if (out_bind_group != nullptr)
    *out_bind_group = 0;
  if (instance == 0 || desc == nullptr || out_bind_group == nullptr ||
      desc->struct_size < sizeof(webgpu_bind_group_desc) || desc->reserved != 0 ||
      desc->layout == 0 || (desc->entry_count != 0 && desc->entries == nullptr)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto layout = state.bind_group_layouts.find(desc->layout);
  if (layout == state.bind_group_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->entry_count != layout->second.entries.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<WGPUBindGroupEntry> entries(desc->entry_count);
    webgpu_device_state::bind_group_record record{nullptr, desc->layout, {}, {}, {}, {}};
    record.entries.assign(desc->entries, desc->entries + desc->entry_count);
    for (std::uint32_t index = 0; index < desc->entry_count; ++index) {
      const auto& source = desc->entries[index];
      if (std::any_of(desc->entries, desc->entries + index,
                      [&](const auto& previous) { return previous.binding == source.binding; }))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto declaration =
          std::find_if(layout->second.entries.begin(), layout->second.entries.end(),
                       [&](const auto& candidate) { return candidate.binding == source.binding; });
      if (declaration == layout->second.entries.end() || declaration->type != source.type)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      auto& entry = entries[index];
      entry = WGPU_BIND_GROUP_ENTRY_INIT;
      entry.binding = source.binding;
      if (source.type == GRANIT_WEBGPU_BINDING_TYPE_UNIFORM_BUFFER ||
          source.type == GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER ||
          source.type == GRANIT_WEBGPU_BINDING_TYPE_STORAGE_BUFFER) {
        const auto buffer = state.buffers.find(source.buffer);
        if (buffer == state.buffers.end())
          return GRANIT_ERROR_INVALID_HANDLE;
        const auto required_usage = source.type == GRANIT_WEBGPU_BINDING_TYPE_STORAGE_BUFFER
                                        ? GRANIT_WEBGPU_BUFFER_USAGE_STORAGE_BIT
                                        : GRANIT_WEBGPU_BUFFER_USAGE_UNIFORM_BIT;
        if (source.offset >= buffer->second.size || source.size == 0 ||
            source.size > buffer->second.size - source.offset ||
            (buffer->second.usage & required_usage) == 0)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.buffer = buffer->second.buffer;
        entry.offset = source.offset;
        entry.size = source.size;
        record.buffers.push_back(source.buffer);
      } else if (source.type == GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_TEXTURE ||
                 source.type == GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_TEXTURE_CUBE ||
                 source.type == GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE) {
        const auto view = state.texture_views.find(source.texture_view);
        if (view == state.texture_views.end())
          return GRANIT_ERROR_INVALID_HANDLE;
        const auto texture = state.textures.find(view->second.texture);
        if (texture == state.textures.end() ||
            (texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_SAMPLED_BIT) == 0)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.textureView = view->second.view;
        record.texture_views.push_back(source.texture_view);
      } else if (source.type == GRANIT_WEBGPU_BINDING_TYPE_SAMPLER ||
                 source.type == GRANIT_WEBGPU_BINDING_TYPE_COMPARISON_SAMPLER) {
        const auto sampler = state.samplers.find(source.sampler);
        if (sampler == state.samplers.end())
          return GRANIT_ERROR_INVALID_HANDLE;
        entry.sampler = sampler->second;
        record.samplers.push_back(source.sampler);
      } else {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    if (record.buffers.size() + record.texture_views.size() + record.samplers.size() !=
        desc->entry_count) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.layout = layout->second.bind_group_layout;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    const auto native = wgpuDeviceCreateBindGroup(state.device, &descriptor);
    if (native == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto handle = next_handle<webgpu_bind_group>(next_bind_group);
    try {
      record.bind_group = native;
      if (!state.bind_groups.emplace(handle, std::move(record)).second) {
        wgpuBindGroupRelease(native);
        return GRANIT_ERROR_INTERNAL;
      }
    } catch (const std::bad_alloc&) {
      wgpuBindGroupRelease(native);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      wgpuBindGroupRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
    *out_bind_group = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_bind_group(webgpu_instance_handle instance,
                                 webgpu_bind_group bind_group) noexcept {
  if (instance == 0 || bind_group == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto group_found = found->second->bind_groups.find(bind_group);
  if (group_found == found->second->bind_groups.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  wgpuBindGroupRelease(group_found->second.bind_group);
  found->second->bind_groups.erase(group_found);
  return GRANIT_SUCCESS;
}

} // namespace

namespace granit::detail {

granit_result webgpu_device::create_buffer(const webgpu_buffer_desc* desc,
                                           webgpu_buffer* buffer) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || buffer == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::create_buffer(instance_, desc, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_buffer(webgpu_buffer buffer) noexcept {
  if (!open_ || instance_ == 0 || buffer == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::destroy_buffer(instance_, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::write_buffer(webgpu_buffer buffer, std::uint64_t offset,
                                          const void* data, std::uint64_t size) noexcept {
  if (!open_ || instance_ == 0 || buffer == 0 || data == nullptr || size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::write_buffer(instance_, buffer, offset, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::read_buffer(webgpu_buffer buffer, std::uint64_t offset, void* data,
                                         std::uint64_t size) noexcept {
  if (!open_ || instance_ == 0 || buffer == 0 || data == nullptr || size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::read_buffer(instance_, buffer, offset, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::begin_readback(webgpu_buffer buffer, std::uint64_t offset,
                                            std::uint64_t size,
                                            webgpu_readback* readback) noexcept {
  if (!open_ || instance_ == 0 || buffer == 0 || size == 0 || readback == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::begin_readback(instance_, buffer, offset, size, readback);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::poll_readback(webgpu_readback readback) noexcept {
  if (!open_ || instance_ == 0 || readback == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::poll_readback(instance_, readback);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::copy_readback(webgpu_readback readback, std::uint64_t offset,
                                           void* data, std::uint64_t size) noexcept {
  if (!open_ || instance_ == 0 || readback == 0 || data == nullptr || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::copy_readback(instance_, readback, offset, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_readback(webgpu_readback readback) noexcept {
  if (!open_ || instance_ == 0 || readback == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_readback(instance_, readback);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_texture(const webgpu_texture_desc* desc,
                                            webgpu_texture* texture) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || texture == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::create_texture(instance_, desc, texture);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_texture(webgpu_texture texture) noexcept {
  if (!open_ || instance_ == 0 || texture == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::destroy_texture(instance_, texture);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::write_texture(webgpu_texture texture,
                                           const webgpu_texture_write_desc* desc, const void* data,
                                           std::uint64_t size) noexcept {
  if (!open_ || instance_ == 0 || texture == 0 || desc == nullptr || data == nullptr || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::write_texture(instance_, texture, desc, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::write_upload_batch(std::span<const webgpu_upload_operation> operations) noexcept {
  if (!open_ || instance_ == 0 || operations.empty() || operations.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::write_upload_batch(instance_, operations.data(),
                                static_cast<std::uint32_t>(operations.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_texture_view(webgpu_texture texture,
                                                 const webgpu_texture_view_desc* desc,
                                                 webgpu_texture_view* view) noexcept {
  if (!open_ || instance_ == 0 || texture == 0 || desc == nullptr || view == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::create_texture_view(instance_, texture, desc, view);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_texture_view(webgpu_texture_view view) noexcept {
  if (!open_ || instance_ == 0 || view == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::destroy_texture_view(instance_, view);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_sampler(const webgpu_sampler_desc* desc,
                                            webgpu_sampler* sampler) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || sampler == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::create_sampler(instance_, desc, sampler);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_sampler(webgpu_sampler sampler) noexcept {
  if (!open_ || instance_ == 0 || sampler == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::destroy_sampler(instance_, sampler);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_bind_group_layout(const webgpu_bind_group_layout_desc* desc,
                                                      webgpu_bind_group_layout* layout) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_bind_group_layout(instance_, desc, layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_bind_group_layout(webgpu_bind_group_layout handle) noexcept {
  if (!open_ || instance_ == 0 || handle == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_bind_group_layout(instance_, handle);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_bind_group(const webgpu_bind_group_desc* desc,
                                               webgpu_bind_group* bind_group) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || bind_group == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_bind_group(instance_, desc, bind_group);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_bind_group(webgpu_bind_group handle) noexcept {
  if (!open_ || instance_ == 0 || handle == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_bind_group(instance_, handle);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
} // namespace granit::detail
