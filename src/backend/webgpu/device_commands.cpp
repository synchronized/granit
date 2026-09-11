// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/webgpu/device_state.h"
#include "backend/webgpu/device_utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

bool valid_transfer_recorder(
    const webgpu_device_state::command_recorder_record& recorder) noexcept {
  return !recorder.finished && recorder.pass == nullptr && recorder.compute_pass == nullptr;
}

bool valid_texture_buffer_copy(const webgpu_device_state::texture_record& texture,
                               const webgpu_device_state::buffer_record& buffer,
                               const webgpu_texture_buffer_copy& region,
                               bool buffer_is_source) noexcept {
  if (region.width == 0 || region.height == 0 || region.depth == 0 ||
      region.array_layer_count == 0 || region.mip_level >= texture.mip_level_count ||
      region.base_array_layer >= texture.array_layer_count ||
      region.array_layer_count > texture.array_layer_count - region.base_array_layer ||
      map_texture_aspect(region.aspect) == WGPUTextureAspect_Undefined)
    return false;
  const auto mip_width = (std::max)(UINT32_C(1), texture.width >> region.mip_level);
  const auto mip_height = (std::max)(UINT32_C(1), texture.height >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z != 0 || region.depth != 1)
    return false;
  const auto block = texture_block(texture.format);
  if (block.bytes == 0)
    return false;
  const std::uint64_t columns = (std::uint64_t{region.width} + block.width - 1) / block.width;
  const std::uint64_t rows = (std::uint64_t{region.height} + block.height - 1) / block.height;
  const std::uint64_t tight_row = columns * block.bytes;
  if (!valid_texture_block_region(texture.format, region.x, region.y, region.width, region.height,
                                  mip_width, mip_height) ||
      region.bytes_per_row < tight_row || region.bytes_per_row % block.bytes != 0 ||
      region.rows_per_image < rows)
    return false;
  const auto max = (std::numeric_limits<std::uint64_t>::max)();
  if (region.rows_per_image > max / region.bytes_per_row)
    return false;
  const auto image_pitch = std::uint64_t{region.rows_per_image} * region.bytes_per_row;
  if (region.array_layer_count - 1 > max / image_pitch || rows - 1 > max / region.bytes_per_row)
    return false;
  const auto required = std::uint64_t{region.array_layer_count - 1} * image_pitch +
                        (rows - 1) * region.bytes_per_row + tight_row;
  if (region.buffer_offset > buffer.size || required > buffer.size - region.buffer_offset)
    return false;
  const auto required_buffer_usage = buffer_is_source ? GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT
                                                      : GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT;
  const auto required_texture_usage = buffer_is_source ? GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT
                                                       : GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT;
  return (buffer.usage & required_buffer_usage) != 0 &&
         (texture.usage & required_texture_usage) != 0;
}

granit_result create_command_recorder(webgpu_instance_handle instance,
                                      webgpu_command_recorder* out_recorder) noexcept {
  if (out_recorder != nullptr)
    *out_recorder = 0;
  if (instance == 0 || out_recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  WGPUCommandEncoderDescriptor descriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
  const auto native = wgpuDeviceCreateCommandEncoder(found->second->device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<webgpu_command_recorder>(next_command_recorder);
  try {
    const auto record = webgpu_device_state::command_recorder_record{
        native, nullptr, nullptr, false, false, false, 0, 0, {}, {}};
    if (!found->second->command_recorders.emplace(handle, record).second) {
      wgpuCommandEncoderRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuCommandEncoderRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuCommandEncoderRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_recorder = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_command_recorder(webgpu_instance_handle instance,
                                       webgpu_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto recorder_found = found->second->command_recorders.find(recorder);
  if (recorder_found == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (recorder_found->second.pass != nullptr)
    wgpuRenderPassEncoderRelease(recorder_found->second.pass);
  if (recorder_found->second.compute_pass != nullptr)
    wgpuComputePassEncoderRelease(recorder_found->second.compute_pass);
  for (const auto buffer : recorder_found->second.temporary_buffers)
    wgpuBufferRelease(buffer);
  wgpuCommandEncoderRelease(recorder_found->second.encoder);
  found->second->command_recorders.erase(recorder_found);
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_buffer_to_texture(webgpu_instance_handle instance,
                                              webgpu_command_recorder recorder,
                                              webgpu_buffer buffer, webgpu_texture texture,
                                              std::uint32_t width, std::uint32_t height,
                                              std::uint32_t bytes_per_row) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 || texture == 0 || width == 0 || height == 0 ||
      bytes_per_row < static_cast<std::uint64_t>(width) * 4 || bytes_per_row % 256 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  const auto buffer_found = state.buffers.find(buffer);
  const auto texture_found = state.textures.find(texture);
  if (recorder_found == state.command_recorders.end() || buffer_found == state.buffers.end() ||
      texture_found == state.textures.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (recorder_found->second.finished || recorder_found->second.pass != nullptr ||
      recorder_found->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto required_size = static_cast<std::uint64_t>(bytes_per_row) * (height - 1) +
                             static_cast<std::uint64_t>(width) * 4;
  if ((buffer_found->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT) == 0 ||
      (texture_found->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
      width > texture_found->second.width || height > texture_found->second.height ||
      required_size > buffer_found->second.size) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPUTexelCopyBufferInfo source = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
  source.buffer = buffer_found->second.buffer;
  source.layout.bytesPerRow = bytes_per_row;
  source.layout.rowsPerImage = height;
  WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  destination.texture = texture_found->second.texture;
  destination.aspect = WGPUTextureAspect_All;
  const WGPUExtent3D extent{width, height, 1};
  wgpuCommandEncoderCopyBufferToTexture(recorder_found->second.encoder, &source, &destination,
                                        &extent);
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_buffer(webgpu_instance_handle instance,
                                   webgpu_command_recorder recorder, webgpu_buffer source,
                                   webgpu_buffer destination,
                                   const webgpu_buffer_copy_region* regions,
                                   std::uint32_t region_count) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || regions == nullptr ||
      region_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto source_buffer = state.buffers.find(source);
  const auto destination_buffer = state.buffers.find(destination);
  if (command == state.command_recorders.end() || source_buffer == state.buffers.end() ||
      destination_buffer == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) ||
      (source_buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT) == 0 ||
      (destination_buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (std::uint32_t index = 0; index < region_count; ++index) {
    const auto& region = regions[index];
    if (region.size == 0 || region.source_offset % 4 != 0 || region.destination_offset % 4 != 0 ||
        region.size % 4 != 0 || region.source_offset > source_buffer->second.size ||
        region.size > source_buffer->second.size - region.source_offset ||
        region.destination_offset > destination_buffer->second.size ||
        region.size > destination_buffer->second.size - region.destination_offset)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < region_count; ++index) {
    const auto& region = regions[index];
    wgpuCommandEncoderCopyBufferToBuffer(command->second.encoder, source_buffer->second.buffer,
                                         region.source_offset, destination_buffer->second.buffer,
                                         region.destination_offset, region.size);
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_buffer_to_texture_v2(
    webgpu_instance_handle instance, webgpu_command_recorder recorder, webgpu_buffer source,
    webgpu_texture destination, const webgpu_texture_buffer_copy* region) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto buffer = state.buffers.find(source);
  const auto texture = state.textures.find(destination);
  if (command == state.command_recorders.end() || buffer == state.buffers.end() ||
      texture == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) ||
      !valid_texture_buffer_copy(texture->second, buffer->second, *region, true))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto encode = [&](std::uint64_t offset, std::uint32_t y, std::uint32_t layer,
                          std::uint32_t height, std::uint32_t layers, bool omit_strides) {
    WGPUTexelCopyBufferInfo native_source = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    native_source.buffer = buffer->second.buffer;
    native_source.layout.offset = offset;
    native_source.layout.bytesPerRow =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->bytes_per_row;
    native_source.layout.rowsPerImage =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->rows_per_image;
    WGPUTexelCopyTextureInfo native_destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    native_destination.texture = texture->second.texture;
    native_destination.mipLevel = region->mip_level;
    native_destination.origin = {region->x, y, layer};
    native_destination.aspect = map_texture_aspect(region->aspect);
    const WGPUExtent3D extent{region->width, height, layers};
    wgpuCommandEncoderCopyBufferToTexture(command->second.encoder, &native_source,
                                          &native_destination, &extent);
  };
  if (region->bytes_per_row % 256 == 0) {
    encode(region->buffer_offset, region->y, region->base_array_layer, region->height,
           region->array_layer_count, false);
  } else {
    const auto block = texture_block(texture->second.format);
    const auto block_rows = (region->height + block.height - 1) / block.height;
    const auto image_pitch = std::uint64_t{region->rows_per_image} * region->bytes_per_row;
    for (std::uint32_t layer = 0; layer < region->array_layer_count; ++layer) {
      for (std::uint32_t row = 0; row < block_rows; ++row) {
        const auto y = region->y + row * block.height;
        const auto height = (std::min)(block.height, region->y + region->height - y);
        encode(region->buffer_offset + std::uint64_t{layer} * image_pitch +
                   std::uint64_t{row} * region->bytes_per_row,
               y, region->base_array_layer + layer, height, 1, true);
      }
    }
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_texture_to_buffer_v2(
    webgpu_instance_handle instance, webgpu_command_recorder recorder, webgpu_texture source,
    webgpu_buffer destination, const webgpu_texture_buffer_copy* region) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto texture = state.textures.find(source);
  const auto buffer = state.buffers.find(destination);
  if (command == state.command_recorders.end() || texture == state.textures.end() ||
      buffer == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) ||
      !valid_texture_buffer_copy(texture->second, buffer->second, *region, false))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto encode = [&](std::uint64_t offset, std::uint32_t y, std::uint32_t layer,
                          std::uint32_t height, std::uint32_t layers, bool omit_strides) {
    WGPUTexelCopyTextureInfo native_source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    native_source.texture = texture->second.texture;
    native_source.mipLevel = region->mip_level;
    native_source.origin = {region->x, y, layer};
    native_source.aspect = map_texture_aspect(region->aspect);
    WGPUTexelCopyBufferInfo native_destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    native_destination.buffer = buffer->second.buffer;
    native_destination.layout.offset = offset;
    native_destination.layout.bytesPerRow =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->bytes_per_row;
    native_destination.layout.rowsPerImage =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->rows_per_image;
    const WGPUExtent3D extent{region->width, height, layers};
    wgpuCommandEncoderCopyTextureToBuffer(command->second.encoder, &native_source,
                                          &native_destination, &extent);
  };
  if (region->bytes_per_row % 256 == 0) {
    encode(region->buffer_offset, region->y, region->base_array_layer, region->height,
           region->array_layer_count, false);
  } else {
    const auto block = texture_block(texture->second.format);
    const auto block_rows = (region->height + block.height - 1) / block.height;
    const auto image_pitch = std::uint64_t{region->rows_per_image} * region->bytes_per_row;
    for (std::uint32_t layer = 0; layer < region->array_layer_count; ++layer) {
      for (std::uint32_t row = 0; row < block_rows; ++row) {
        const auto y = region->y + row * block.height;
        const auto height = (std::min)(block.height, region->y + region->height - y);
        encode(region->buffer_offset + std::uint64_t{layer} * image_pitch +
                   std::uint64_t{row} * region->bytes_per_row,
               y, region->base_array_layer + layer, height, 1, true);
      }
    }
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_texture(webgpu_instance_handle instance,
                                    webgpu_command_recorder recorder, webgpu_texture source,
                                    webgpu_texture destination,
                                    const webgpu_texture_copy_region* region) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || region == nullptr ||
      region->width == 0 || region->height == 0 || region->depth == 0 ||
      region->array_layer_count == 0 ||
      map_texture_aspect(region->aspect) == WGPUTextureAspect_Undefined)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto source_texture = state.textures.find(source);
  const auto destination_texture = state.textures.find(destination);
  if (command == state.command_recorders.end() || source_texture == state.textures.end() ||
      destination_texture == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& source_desc = source_texture->second;
  const auto& destination_desc = destination_texture->second;
  if (!valid_transfer_recorder(command->second) || source_desc.format != destination_desc.format ||
      (source_desc.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (destination_desc.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
      region->source_mip_level >= source_desc.mip_level_count ||
      region->destination_mip_level >= destination_desc.mip_level_count ||
      region->source_base_array_layer >= source_desc.array_layer_count ||
      region->array_layer_count > source_desc.array_layer_count - region->source_base_array_layer ||
      region->destination_base_array_layer >= destination_desc.array_layer_count ||
      region->array_layer_count >
          destination_desc.array_layer_count - region->destination_base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto source_width = (std::max)(UINT32_C(1), source_desc.width >> region->source_mip_level);
  const auto source_height =
      (std::max)(UINT32_C(1), source_desc.height >> region->source_mip_level);
  const auto destination_width =
      (std::max)(UINT32_C(1), destination_desc.width >> region->destination_mip_level);
  const auto destination_height =
      (std::max)(UINT32_C(1), destination_desc.height >> region->destination_mip_level);
  if (region->source_x >= source_width || region->width > source_width - region->source_x ||
      region->source_y >= source_height || region->height > source_height - region->source_y ||
      region->destination_x >= destination_width ||
      region->width > destination_width - region->destination_x ||
      region->destination_y >= destination_height ||
      region->height > destination_height - region->destination_y || region->source_z != 0 ||
      region->destination_z != 0 || region->depth != 1)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUTexelCopyTextureInfo native_source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  native_source.texture = source_desc.texture;
  native_source.mipLevel = region->source_mip_level;
  native_source.origin = {region->source_x, region->source_y, region->source_base_array_layer};
  native_source.aspect = map_texture_aspect(region->aspect);
  WGPUTexelCopyTextureInfo native_destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  native_destination.texture = destination_desc.texture;
  native_destination.mipLevel = region->destination_mip_level;
  native_destination.origin = {region->destination_x, region->destination_y,
                               region->destination_base_array_layer};
  native_destination.aspect = map_texture_aspect(region->aspect);
  const WGPUExtent3D extent{region->width, region->height, region->array_layer_count};
  wgpuCommandEncoderCopyTextureToTexture(command->second.encoder, &native_source,
                                         &native_destination, &extent);
  return GRANIT_SUCCESS;
}

granit_result recorder_fill_buffer(webgpu_instance_handle instance,
                                   webgpu_command_recorder recorder, webgpu_buffer buffer,
                                   std::uint64_t offset, std::uint64_t size,
                                   std::uint32_t value) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 || size == 0 || offset % 4 != 0 ||
      size % 4 != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto destination = state.buffers.find(buffer);
  if (command == state.command_recorders.end() || destination == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) || offset > destination->second.size ||
      size > destination->second.size - offset ||
      (destination->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (value == 0) {
    wgpuCommandEncoderClearBuffer(command->second.encoder, destination->second.buffer, offset,
                                  size);
    return GRANIT_SUCCESS;
  }
  if (size > (std::numeric_limits<std::size_t>::max)())
    return GRANIT_ERROR_UNSUPPORTED;
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.usage = WGPUBufferUsage_CopySrc;
  descriptor.size = size;
  descriptor.mappedAtCreation = true;
  const auto staging = wgpuDeviceCreateBuffer(state.device, &descriptor);
  if (staging == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  auto* words = static_cast<std::uint32_t*>(
      wgpuBufferGetMappedRange(staging, 0, static_cast<std::size_t>(size)));
  if (words == nullptr) {
    wgpuBufferRelease(staging);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  std::fill_n(words, static_cast<std::size_t>(size / 4), value);
  wgpuBufferUnmap(staging);
  try {
    command->second.temporary_buffers.push_back(staging);
  } catch (const std::bad_alloc&) {
    wgpuBufferRelease(staging);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuBufferRelease(staging);
    return GRANIT_ERROR_INTERNAL;
  }
  wgpuCommandEncoderCopyBufferToBuffer(command->second.encoder, staging, 0,
                                       destination->second.buffer, offset, size);
  return GRANIT_SUCCESS;
}

granit_result recorder_generate_mipmaps(webgpu_instance_handle instance,
                                        webgpu_command_recorder recorder, webgpu_texture texture,
                                        const webgpu_texture_mipmap_range* range) noexcept {
  if (instance == 0 || recorder == 0 || texture == 0 || range == nullptr ||
      range->level_count < 2 || range->array_layer_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto texture_found = state.textures.find(texture);
  if (command == state.command_recorders.end() || texture_found == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& record = texture_found->second;
  if (!valid_transfer_recorder(command->second) || record.borrowed || record.sample_count != 1 ||
      record.format == GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT ||
      range->base_mip_level >= record.mip_level_count ||
      range->level_count > record.mip_level_count - range->base_mip_level ||
      range->base_array_layer >= record.array_layer_count ||
      range->array_layer_count > record.array_layer_count - range->base_array_layer ||
      (record.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (record.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  constexpr char mipmap_wgsl[] = R"(
struct vertex_output {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};
@group(0) @binding(0) var source_texture: texture_2d<f32>;
@group(0) @binding(1) var source_sampler: sampler;
@vertex fn vs_main(@builtin(vertex_index) index: u32) -> vertex_output {
  let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  let coordinates = array<vec2f, 3>(vec2f(0.0, 1.0), vec2f(2.0, 1.0), vec2f(0.0, -1.0));
  var output: vertex_output;
  output.position = vec4f(positions[index], 0.0, 1.0);
  output.uv = coordinates[index];
  return output;
}
@fragment fn fs_main(input: vertex_output) -> @location(0) vec4f {
  return textureSampleLevel(source_texture, source_sampler, input.uv, 0.0);
})";
  WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
  source.code = {mipmap_wgsl, sizeof(mipmap_wgsl) - 1};
  WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  shader_desc.nextInChain = &source.chain;
  const auto shader = wgpuDeviceCreateShaderModule(state.device, &shader_desc);
  if (shader == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;

  WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
  target.format = to_native_texture_format(record.format);
  target.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = shader;
  fragment.entryPoint = {"fs_main", 7};
  fragment.targetCount = 1;
  fragment.targets = &target;
  WGPURenderPipelineDescriptor pipeline_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  pipeline_desc.vertex.module = shader;
  pipeline_desc.vertex.entryPoint = {"vs_main", 7};
  pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipeline_desc.primitive.frontFace = WGPUFrontFace_CCW;
  pipeline_desc.primitive.cullMode = WGPUCullMode_None;
  pipeline_desc.multisample.count = 1;
  pipeline_desc.multisample.mask = UINT32_MAX;
  pipeline_desc.fragment = &fragment;
  const auto pipeline = wgpuDeviceCreateRenderPipeline(state.device, &pipeline_desc);
  if (pipeline == nullptr) {
    wgpuShaderModuleRelease(shader);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  const auto bind_group_layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
  WGPUSamplerDescriptor sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
  sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
  sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
  sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
  sampler_desc.magFilter = WGPUFilterMode_Linear;
  sampler_desc.minFilter = WGPUFilterMode_Linear;
  sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  const auto sampler = wgpuDeviceCreateSampler(state.device, &sampler_desc);
  if (bind_group_layout == nullptr || sampler == nullptr) {
    if (sampler != nullptr)
      wgpuSamplerRelease(sampler);
    if (bind_group_layout != nullptr)
      wgpuBindGroupLayoutRelease(bind_group_layout);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(shader);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  for (std::uint32_t layer = range->base_array_layer;
       layer < range->base_array_layer + range->array_layer_count; ++layer) {
    for (std::uint32_t level = range->base_mip_level + 1;
         level < range->base_mip_level + range->level_count; ++level) {
      WGPUTextureViewDescriptor source_view_desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
      source_view_desc.format = target.format;
      source_view_desc.dimension = WGPUTextureViewDimension_2D;
      source_view_desc.baseMipLevel = level - 1;
      source_view_desc.mipLevelCount = 1;
      source_view_desc.baseArrayLayer = layer;
      source_view_desc.arrayLayerCount = 1;
      source_view_desc.aspect = WGPUTextureAspect_All;
      WGPUTextureViewDescriptor destination_view_desc = source_view_desc;
      destination_view_desc.baseMipLevel = level;
      const auto source_view = wgpuTextureCreateView(record.texture, &source_view_desc);
      const auto destination_view = wgpuTextureCreateView(record.texture, &destination_view_desc);
      if (source_view == nullptr || destination_view == nullptr) {
        if (source_view != nullptr)
          wgpuTextureViewRelease(source_view);
        if (destination_view != nullptr)
          wgpuTextureViewRelease(destination_view);
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(bind_group_layout);
        wgpuRenderPipelineRelease(pipeline);
        wgpuShaderModuleRelease(shader);
        return GRANIT_ERROR_OUT_OF_MEMORY;
      }
      WGPUBindGroupEntry entries[2]{};
      entries[0].binding = 0;
      entries[0].textureView = source_view;
      entries[1].binding = 1;
      entries[1].sampler = sampler;
      WGPUBindGroupDescriptor bind_group_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
      bind_group_desc.layout = bind_group_layout;
      bind_group_desc.entryCount = 2;
      bind_group_desc.entries = entries;
      const auto bind_group = wgpuDeviceCreateBindGroup(state.device, &bind_group_desc);
      if (bind_group == nullptr) {
        wgpuTextureViewRelease(destination_view);
        wgpuTextureViewRelease(source_view);
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(bind_group_layout);
        wgpuRenderPipelineRelease(pipeline);
        wgpuShaderModuleRelease(shader);
        return GRANIT_ERROR_INITIALIZATION_FAILED;
      }
      WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
      color.view = destination_view;
      color.loadOp = WGPULoadOp_Clear;
      color.storeOp = WGPUStoreOp_Store;
      WGPURenderPassDescriptor pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
      pass_desc.colorAttachmentCount = 1;
      pass_desc.colorAttachments = &color;
      const auto pass = wgpuCommandEncoderBeginRenderPass(command->second.encoder, &pass_desc);
      if (pass == nullptr) {
        wgpuBindGroupRelease(bind_group);
        wgpuTextureViewRelease(destination_view);
        wgpuTextureViewRelease(source_view);
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(bind_group_layout);
        wgpuRenderPipelineRelease(pipeline);
        wgpuShaderModuleRelease(shader);
        return GRANIT_ERROR_INITIALIZATION_FAILED;
      }
      wgpuRenderPassEncoderSetPipeline(pass, pipeline);
      wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group, 0, nullptr);
      wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
      wgpuRenderPassEncoderEnd(pass);
      wgpuRenderPassEncoderRelease(pass);
      wgpuBindGroupRelease(bind_group);
      wgpuTextureViewRelease(destination_view);
      wgpuTextureViewRelease(source_view);
    }
  }
  wgpuSamplerRelease(sampler);
  wgpuBindGroupLayoutRelease(bind_group_layout);
  wgpuRenderPipelineRelease(pipeline);
  wgpuShaderModuleRelease(shader);
  return GRANIT_SUCCESS;
}

granit_result recorder_begin_rendering(
    webgpu_instance_handle instance, webgpu_command_recorder recorder, webgpu_texture_view target,
    webgpu_texture_view resolve_target, webgpu_load_operation load_operation,
    webgpu_store_operation store_operation, float clear_r, float clear_g, float clear_b,
    float clear_a, webgpu_texture_view depth_target, webgpu_load_operation depth_load_operation,
    webgpu_store_operation depth_store_operation, float clear_depth) noexcept {
  if (instance == 0 || recorder == 0 || (target == 0 && depth_target == 0) ||
      (resolve_target != 0 && target == 0) ||
      (target != 0 && ((load_operation != GRANIT_WEBGPU_LOAD_OPERATION_LOAD &&
                        load_operation != GRANIT_WEBGPU_LOAD_OPERATION_CLEAR) ||
                       (store_operation != GRANIT_WEBGPU_STORE_OPERATION_STORE &&
                        store_operation != GRANIT_WEBGPU_STORE_OPERATION_DISCARD))) ||
      (depth_target != 0 &&
       ((depth_load_operation != GRANIT_WEBGPU_LOAD_OPERATION_LOAD &&
         depth_load_operation != GRANIT_WEBGPU_LOAD_OPERATION_CLEAR) ||
        (depth_store_operation != GRANIT_WEBGPU_STORE_OPERATION_STORE &&
         depth_store_operation != GRANIT_WEBGPU_STORE_OPERATION_DISCARD) ||
        !std::isfinite(clear_depth) || clear_depth < 0.0F || clear_depth > 1.0F)))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto view = state.texture_views.find(target);
  const auto resolve_view = state.texture_views.find(resolve_target);
  const auto depth_view = state.texture_views.find(depth_target);
  if (command == state.command_recorders.end() ||
      (target != 0 && view == state.texture_views.end()) ||
      (resolve_target != 0 && resolve_view == state.texture_views.end())) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto texture =
      target == 0 ? state.textures.end() : state.textures.find(view->second.texture);
  const auto resolve_texture = resolve_target == 0
                                   ? state.textures.end()
                                   : state.textures.find(resolve_view->second.texture);
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr ||
      (target != 0 &&
       (texture == state.textures.end() ||
        (texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) == 0)) ||
      (resolve_target != 0 &&
       (resolve_texture == state.textures.end() || texture->second.sample_count != 4 ||
        resolve_texture->second.sample_count != 1 ||
        texture->second.format != resolve_texture->second.format ||
        texture->second.width != resolve_texture->second.width ||
        texture->second.height != resolve_texture->second.height ||
        (resolve_texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) ==
            0))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPURenderPassDepthStencilAttachment depth_attachment =
      WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  if (depth_target != 0) {
    if (depth_view == state.texture_views.end()) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto depth_texture = state.textures.find(depth_view->second.texture);
    if (depth_texture == state.textures.end() ||
        depth_texture->second.format != GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT ||
        (depth_texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) == 0) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    depth_attachment.view = depth_view->second.view;
    depth_attachment.depthLoadOp = depth_load_operation == GRANIT_WEBGPU_LOAD_OPERATION_LOAD
                                       ? WGPULoadOp_Load
                                       : WGPULoadOp_Clear;
    depth_attachment.depthStoreOp = depth_store_operation == GRANIT_WEBGPU_STORE_OPERATION_STORE
                                        ? WGPUStoreOp_Store
                                        : WGPUStoreOp_Discard;
    depth_attachment.depthClearValue = clear_depth;
    depth_attachment.depthReadOnly = false;
    depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
    depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    depth_attachment.stencilReadOnly = true;
  }
  WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
  if (target != 0) {
    color.view = view->second.view;
    color.resolveTarget = resolve_target == 0 ? nullptr : resolve_view->second.view;
    color.loadOp =
        load_operation == GRANIT_WEBGPU_LOAD_OPERATION_LOAD ? WGPULoadOp_Load : WGPULoadOp_Clear;
    color.storeOp = store_operation == GRANIT_WEBGPU_STORE_OPERATION_STORE ? WGPUStoreOp_Store
                                                                           : WGPUStoreOp_Discard;
    color.clearValue = {clear_r, clear_g, clear_b, clear_a};
  }
  WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
  descriptor.colorAttachmentCount = target == 0 ? 0 : 1;
  descriptor.colorAttachments = target == 0 ? nullptr : &color;
  descriptor.depthStencilAttachment = depth_target == 0 ? nullptr : &depth_attachment;
  command->second.pass = wgpuCommandEncoderBeginRenderPass(command->second.encoder, &descriptor);
  if (command->second.pass == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  command->second.pipeline_bound = false;
  command->second.index_available = 0;
  command->second.index_element_size = 0;
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_pipeline(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder,
                                     webgpu_render_pipeline pipeline) noexcept {
  if (instance == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto native = state.render_pipelines.find(pipeline);
  if (command == state.command_recorders.end() || native == state.render_pipelines.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetPipeline(command->second.pass, native->second.render_pipeline);
  command->second.pipeline_bound = true;
  return GRANIT_SUCCESS;
}

granit_result
recorder_bind_graphics_groups(webgpu_instance_handle instance, webgpu_command_recorder recorder,
                              webgpu_pipeline_layout pipeline_layout, std::uint32_t first_group,
                              const webgpu_bind_group* groups, std::uint32_t group_count,
                              const std::uint32_t* dynamic_offsets,
                              std::uint32_t dynamic_offset_count) noexcept {
  if (instance == 0 || recorder == 0 || pipeline_layout == 0 || group_count == 0 ||
      groups == nullptr || first_group > UINT32_MAX - group_count ||
      (dynamic_offset_count != 0 && dynamic_offsets == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto layout = state.pipeline_layouts.find(pipeline_layout);
  if (command == state.command_recorders.end() || layout == state.pipeline_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished ||
      first_group > layout->second.bind_group_layouts.size() ||
      group_count > layout->second.bind_group_layouts.size() - first_group)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::uint32_t offset_index = 0;
  for (std::uint32_t group_index = 0; group_index < group_count; ++group_index) {
    const auto group = state.bind_groups.find(groups[group_index]);
    if (group == state.bind_groups.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if (group->second.layout != layout->second.bind_group_layouts[first_group + group_index])
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto declarations = state.bind_group_layouts.find(group->second.layout);
    if (declarations == state.bind_group_layouts.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    for (const auto& declaration : declarations->second.entries) {
      if (declaration.type != GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
        continue;
      if (offset_index >= dynamic_offset_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto write =
          std::find_if(group->second.entries.begin(), group->second.entries.end(),
                       [&](const auto& entry) { return entry.binding == declaration.binding; });
      if (write == group->second.entries.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto buffer = state.buffers.find(write->buffer);
      const auto dynamic_offset = static_cast<std::uint64_t>(dynamic_offsets[offset_index++]);
      if (buffer == state.buffers.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((state.capabilities.uniform_buffer_offset_alignment != 0 &&
           dynamic_offset % state.capabilities.uniform_buffer_offset_alignment != 0) ||
          write->offset > buffer->second.size ||
          dynamic_offset > buffer->second.size - write->offset ||
          write->size > buffer->second.size - write->offset - dynamic_offset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  if (offset_index != dynamic_offset_count)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  offset_index = 0;
  for (std::uint32_t group_index = 0; group_index < group_count; ++group_index) {
    const auto& group = state.bind_groups.find(groups[group_index])->second;
    const auto& declarations = state.bind_group_layouts.find(group.layout)->second.entries;
    const auto count = static_cast<std::uint32_t>(
        std::count_if(declarations.begin(), declarations.end(), [](const auto& declaration) {
          return declaration.type == GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        }));
    wgpuRenderPassEncoderSetBindGroup(command->second.pass, first_group + group_index,
                                      group.bind_group, count,
                                      count == 0 ? nullptr : dynamic_offsets + offset_index);
    offset_index += count;
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_vertex_buffers(webgpu_instance_handle instance,
                                           webgpu_command_recorder recorder, std::uint32_t first,
                                           const webgpu_vertex_buffer_binding* bindings,
                                           std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || count == 0 || bindings == nullptr ||
      first > UINT32_MAX - count)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  if (command == state.command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto buffer = state.buffers.find(bindings[index].buffer);
    if (buffer == state.buffers.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if ((buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_VERTEX_BIT) == 0 ||
        bindings[index].offset >= buffer->second.size)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto& binding = bindings[index];
    const auto& buffer = state.buffers.find(binding.buffer)->second;
    wgpuRenderPassEncoderSetVertexBuffer(command->second.pass, first + index, buffer.buffer,
                                         binding.offset, buffer.size - binding.offset);
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_index_buffer(webgpu_instance_handle instance,
                                         webgpu_command_recorder recorder, webgpu_buffer buffer,
                                         std::uint64_t offset,
                                         webgpu_index_format format) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 ||
      (format != GRANIT_WEBGPU_INDEX_FORMAT_UINT16 && format != GRANIT_WEBGPU_INDEX_FORMAT_UINT32))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto native = state.buffers.find(buffer);
  if (command == state.command_recorders.end() || native == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto element_size = format == GRANIT_WEBGPU_INDEX_FORMAT_UINT16 ? 2U : 4U;
  if (command->second.pass == nullptr || command->second.finished ||
      (native->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_INDEX_BIT) == 0 ||
      offset >= native->second.size || offset % element_size != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetIndexBuffer(
      command->second.pass, native->second.buffer,
      format == GRANIT_WEBGPU_INDEX_FORMAT_UINT16 ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32,
      offset, native->second.size - offset);
  command->second.index_available = native->second.size - offset;
  command->second.index_element_size = element_size;
  return GRANIT_SUCCESS;
}

granit_result recorder_set_viewports(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder, std::uint32_t first,
                                     const webgpu_viewport* viewports,
                                     std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || first != 0 || viewports == nullptr || count != 1)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& viewport = viewports[0];
  if (viewport.width <= 0.0F || viewport.height <= 0.0F || viewport.min_depth < 0.0F ||
      viewport.max_depth > 1.0F || viewport.min_depth > viewport.max_depth)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetViewport(command->second.pass, viewport.x, viewport.y, viewport.width,
                                   viewport.height, viewport.min_depth, viewport.max_depth);
  return GRANIT_SUCCESS;
}

granit_result recorder_set_scissors(webgpu_instance_handle instance,
                                    webgpu_command_recorder recorder, std::uint32_t first,
                                    const webgpu_scissor* scissors, std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || first != 0 || scissors == nullptr || count != 1 ||
      scissors[0].width == 0 || scissors[0].height == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& scissor = scissors[0];
  wgpuRenderPassEncoderSetScissorRect(command->second.pass, scissor.x, scissor.y, scissor.width,
                                      scissor.height);
  return GRANIT_SUCCESS;
}

granit_result recorder_draw_vertices(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder, std::uint32_t vertex_count,
                                     std::uint32_t instance_count, std::uint32_t first_vertex,
                                     std::uint32_t first_instance) noexcept {
  if (instance == 0 || recorder == 0 || vertex_count == 0 || instance_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || !command->second.pipeline_bound ||
      command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderDraw(command->second.pass, vertex_count, instance_count, first_vertex,
                            first_instance);
  return GRANIT_SUCCESS;
}

granit_result recorder_draw_indices(webgpu_instance_handle instance,
                                    webgpu_command_recorder recorder, std::uint32_t index_count,
                                    std::uint32_t instance_count, std::uint32_t first_index,
                                    std::int32_t vertex_offset,
                                    std::uint32_t first_instance) noexcept {
  if (instance == 0 || recorder == 0 || index_count == 0 || instance_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto first_byte =
      static_cast<std::uint64_t>(first_index) * command->second.index_element_size;
  const auto draw_size =
      static_cast<std::uint64_t>(index_count) * command->second.index_element_size;
  if (command->second.pass == nullptr || !command->second.pipeline_bound ||
      command->second.finished || command->second.index_element_size == 0 ||
      first_byte > command->second.index_available ||
      draw_size > command->second.index_available - first_byte)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderDrawIndexed(command->second.pass, index_count, instance_count, first_index,
                                   vertex_offset, first_instance);
  return GRANIT_SUCCESS;
}

granit_result recorder_end_rendering(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderEnd(command->second.pass);
  wgpuRenderPassEncoderRelease(command->second.pass);
  command->second.pass = nullptr;
  command->second.pipeline_bound = false;
  command->second.index_available = 0;
  command->second.index_element_size = 0;
  return GRANIT_SUCCESS;
}

granit_result recorder_begin_compute(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUComputePassDescriptor descriptor = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
  command->second.compute_pass =
      wgpuCommandEncoderBeginComputePass(command->second.encoder, &descriptor);
  if (command->second.compute_pass == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  command->second.compute_pipeline_bound = false;
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_compute_pipeline(webgpu_instance_handle instance,
                                             webgpu_command_recorder recorder,
                                             webgpu_compute_pipeline pipeline) noexcept {
  if (instance == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto native = state.compute_pipelines.find(pipeline);
  if (command == state.command_recorders.end() || native == state.compute_pipelines.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuComputePassEncoderSetPipeline(command->second.compute_pass, native->second.compute_pipeline);
  command->second.compute_pipeline_bound = true;
  return GRANIT_SUCCESS;
}

granit_result
recorder_bind_compute_groups(webgpu_instance_handle instance, webgpu_command_recorder recorder,
                             webgpu_pipeline_layout pipeline_layout, std::uint32_t first_group,
                             const webgpu_bind_group* groups, std::uint32_t group_count,
                             const std::uint32_t* dynamic_offsets,
                             std::uint32_t dynamic_offset_count) noexcept {
  if (instance == 0 || recorder == 0 || pipeline_layout == 0 || groups == nullptr ||
      group_count == 0 || (dynamic_offset_count != 0 && dynamic_offsets == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto layout = state.pipeline_layouts.find(pipeline_layout);
  if (command == state.command_recorders.end() || layout == state.pipeline_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr ||
      first_group > layout->second.bind_group_layouts.size() ||
      group_count > layout->second.bind_group_layouts.size() - first_group)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::uint32_t offset_index{};
  for (std::uint32_t index = 0; index < group_count; ++index) {
    const auto group = state.bind_groups.find(groups[index]);
    if (group == state.bind_groups.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if (group->second.layout != layout->second.bind_group_layouts[first_group + index])
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto& declarations = state.bind_group_layouts.find(group->second.layout)->second.entries;
    const auto group_offset_begin = offset_index;
    for (const auto& declaration : declarations) {
      if (declaration.type != GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
        continue;
      if (offset_index >= dynamic_offset_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto write =
          std::find_if(group->second.entries.begin(), group->second.entries.end(),
                       [&](const auto& entry) { return entry.binding == declaration.binding; });
      if (write == group->second.entries.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto buffer = state.buffers.find(write->buffer);
      const auto dynamic_offset = static_cast<std::uint64_t>(dynamic_offsets[offset_index++]);
      if (buffer == state.buffers.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((state.capabilities.uniform_buffer_offset_alignment != 0 &&
           dynamic_offset % state.capabilities.uniform_buffer_offset_alignment != 0) ||
          write->offset > buffer->second.size ||
          dynamic_offset > buffer->second.size - write->offset ||
          write->size > buffer->second.size - write->offset - dynamic_offset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto count = offset_index - group_offset_begin;
    wgpuComputePassEncoderSetBindGroup(command->second.compute_pass, first_group + index,
                                       group->second.bind_group, count,
                                       count == 0 ? nullptr : dynamic_offsets + group_offset_begin);
  }
  return offset_index == dynamic_offset_count ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result recorder_dispatch(webgpu_instance_handle instance, webgpu_command_recorder recorder,
                                std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
  if (instance == 0 || recorder == 0 || x == 0 || y == 0 || z == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr ||
      !command->second.compute_pipeline_bound)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuComputePassEncoderDispatchWorkgroups(command->second.compute_pass, x, y, z);
  return GRANIT_SUCCESS;
}

granit_result recorder_end_compute(webgpu_instance_handle instance,
                                   webgpu_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuComputePassEncoderEnd(command->second.compute_pass);
  wgpuComputePassEncoderRelease(command->second.compute_pass);
  command->second.compute_pass = nullptr;
  command->second.compute_pipeline_bound = false;
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_texture_to_buffer(webgpu_instance_handle instance,
                                              webgpu_command_recorder recorder,
                                              webgpu_texture texture, webgpu_buffer buffer,
                                              std::uint32_t width, std::uint32_t height,
                                              std::uint32_t bytes_per_row) noexcept {
  if (instance == 0 || recorder == 0 || texture == 0 || buffer == 0 || width == 0 || height == 0 ||
      bytes_per_row < static_cast<std::uint64_t>(width) * 4 || bytes_per_row % 256 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  const auto texture_found = state.textures.find(texture);
  const auto buffer_found = state.buffers.find(buffer);
  if (recorder_found == state.command_recorders.end() || texture_found == state.textures.end() ||
      buffer_found == state.buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (recorder_found->second.finished || recorder_found->second.pass != nullptr ||
      recorder_found->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto required_size = static_cast<std::uint64_t>(bytes_per_row) * (height - 1) +
                             static_cast<std::uint64_t>(width) * 4;
  if ((texture_found->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (buffer_found->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0 ||
      width > texture_found->second.width || height > texture_found->second.height ||
      required_size > buffer_found->second.size) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  source.texture = texture_found->second.texture;
  source.aspect = WGPUTextureAspect_All;
  WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
  destination.buffer = buffer_found->second.buffer;
  destination.layout.bytesPerRow = bytes_per_row;
  destination.layout.rowsPerImage = height;
  const WGPUExtent3D extent{width, height, 1};
  wgpuCommandEncoderCopyTextureToBuffer(recorder_found->second.encoder, &source, &destination,
                                        &extent);
  return GRANIT_SUCCESS;
}

granit_result finish_command_recorder(webgpu_instance_handle instance,
                                      webgpu_command_recorder recorder,
                                      webgpu_command_buffer* out_command_buffer) noexcept {
  if (out_command_buffer != nullptr)
    *out_command_buffer = 0;
  if (instance == 0 || recorder == 0 || out_command_buffer == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  if (recorder_found == state.command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (recorder_found->second.finished || recorder_found->second.pass != nullptr ||
      recorder_found->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (const auto pool : recorder_found->second.timestamp_pools) {
    const auto query = state.timestamp_queries.find(pool);
    if (query == state.timestamp_queries.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto size = static_cast<std::uint64_t>(query->second->count) * sizeof(std::uint64_t);
    wgpuCommandEncoderResolveQuerySet(recorder_found->second.encoder, query->second->query_set, 0,
                                      query->second->count, query->second->resolve_buffer, 0);
    wgpuCommandEncoderCopyBufferToBuffer(recorder_found->second.encoder,
                                         query->second->resolve_buffer, 0,
                                         query->second->read_buffer, 0, size);
  }
  WGPUCommandBufferDescriptor descriptor = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
  const auto native = wgpuCommandEncoderFinish(recorder_found->second.encoder, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  for (const auto buffer : recorder_found->second.temporary_buffers)
    wgpuBufferRelease(buffer);
  recorder_found->second.temporary_buffers.clear();
  const auto handle = next_handle<webgpu_command_buffer>(next_command_buffer);
  try {
    if (!state.command_buffers.emplace(handle, native).second) {
      wgpuCommandBufferRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuCommandBufferRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuCommandBufferRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  recorder_found->second.finished = true;
  *out_command_buffer = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_command_buffer(webgpu_instance_handle instance,
                                     webgpu_command_buffer command_buffer) noexcept {
  if (instance == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto buffer_found = found->second->command_buffers.find(command_buffer);
  if (buffer_found == found->second->command_buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  wgpuCommandBufferRelease(buffer_found->second);
  found->second->command_buffers.erase(buffer_found);
  return GRANIT_SUCCESS;
}

granit_result submit_command_buffer(webgpu_instance_handle instance,
                                    webgpu_command_buffer command_buffer) noexcept {
  if (instance == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto buffer_found = found->second->command_buffers.find(command_buffer);
  if (buffer_found == found->second->command_buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const WGPUCommandBuffer native[]{buffer_found->second};
  wgpuQueueSubmit(found->second->queue, 1, native);
  wgpuCommandBufferRelease(buffer_found->second);
  found->second->command_buffers.erase(buffer_found);
  return GRANIT_SUCCESS;
}

} // namespace

namespace granit::detail {

granit_result webgpu_device::recorder_begin_compute(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_begin_compute(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_bind_compute_pipeline(webgpu_command_recorder recorder,
                                              webgpu_compute_pipeline pipeline) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_compute_pipeline(instance_, recorder, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_compute_groups(
    webgpu_command_recorder recorder, webgpu_pipeline_layout layout, std::uint32_t first_group,
    std::span<const webgpu_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || layout == 0 || groups.empty() ||
      groups.size() > UINT32_MAX || dynamic_offsets.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_compute_groups(instance_, recorder, layout, first_group, groups.data(),
                                          static_cast<std::uint32_t>(groups.size()),
                                          dynamic_offsets.data(),
                                          static_cast<std::uint32_t>(dynamic_offsets.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_dispatch(webgpu_command_recorder recorder, std::uint32_t x,
                                               std::uint32_t y, std::uint32_t z) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || x == 0 || y == 0 || z == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_dispatch(instance_, recorder, x, y, z);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_end_compute(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_end_compute(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
granit_result webgpu_device::create_command_recorder(webgpu_command_recorder* recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_command_recorder(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_command_recorder(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_command_recorder(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_buffer_to_texture(
    webgpu_command_recorder recorder, webgpu_buffer buffer, webgpu_texture texture,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || buffer == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_buffer_to_texture(instance_, recorder, buffer, texture, width, height,
                                             bytes_per_row);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_begin_rendering(
    webgpu_command_recorder recorder, webgpu_texture_view target, webgpu_load_operation load,
    webgpu_store_operation store, const float clear[4], webgpu_texture_view resolve_target,
    webgpu_texture_view depth_target, webgpu_load_operation depth_load,
    webgpu_store_operation depth_store, float clear_depth) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || (target == 0 && depth_target == 0) ||
      clear == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_begin_rendering(instance_, recorder, target, resolve_target, load, store,
                                      clear[0], clear[1], clear[2], clear[3], depth_target,
                                      depth_load, depth_store, clear_depth);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_pipeline(webgpu_command_recorder recorder,
                                                    webgpu_render_pipeline pipeline) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_pipeline(instance_, recorder, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_graphics_groups(
    webgpu_command_recorder recorder, webgpu_pipeline_layout layout, std::uint32_t first_group,
    std::span<const webgpu_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || layout == 0 || groups.empty() ||
      groups.size() > UINT32_MAX || dynamic_offsets.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_graphics_groups(instance_, recorder, layout, first_group, groups.data(),
                                           static_cast<std::uint32_t>(groups.size()),
                                           dynamic_offsets.data(),
                                           static_cast<std::uint32_t>(dynamic_offsets.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_vertex_buffers(
    webgpu_command_recorder recorder, std::uint32_t first,
    std::span<const webgpu_vertex_buffer_binding> bindings) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_vertex_buffers(instance_, recorder, first, bindings.data(),
                                          static_cast<std::uint32_t>(bindings.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_index_buffer(webgpu_command_recorder recorder,
                                                        webgpu_buffer buffer, std::uint64_t offset,
                                                        webgpu_index_format format) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_index_buffer(instance_, recorder, buffer, offset, format);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_set_viewports(webgpu_command_recorder recorder, std::uint32_t first,
                                      std::span<const webgpu_viewport> viewports) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || viewports.empty() ||
      viewports.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_set_viewports(instance_, recorder, first, viewports.data(),
                                    static_cast<std::uint32_t>(viewports.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_set_scissors(webgpu_command_recorder recorder, std::uint32_t first,
                                     std::span<const webgpu_scissor> scissors) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || scissors.empty() || scissors.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_set_scissors(instance_, recorder, first, scissors.data(),
                                   static_cast<std::uint32_t>(scissors.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_draw_vertices(webgpu_command_recorder recorder,
                                                    std::uint32_t vertex_count,
                                                    std::uint32_t instance_count,
                                                    std::uint32_t first_vertex,
                                                    std::uint32_t first_instance) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_draw_vertices(instance_, recorder, vertex_count, instance_count, first_vertex,
                                    first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_draw_indices(
    webgpu_command_recorder recorder, std::uint32_t index_count, std::uint32_t instance_count,
    std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_draw_indices(instance_, recorder, index_count, instance_count, first_index,
                                   vertex_offset, first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_end_rendering(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_end_rendering(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::finish_command_recorder(webgpu_command_recorder recorder,
                                       webgpu_command_buffer* command_buffer) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || command_buffer == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::finish_command_recorder(instance_, recorder, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_command_buffer(webgpu_command_buffer command_buffer) noexcept {
  if (!open_ || instance_ == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_command_buffer(instance_, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::submit_command_buffer(webgpu_command_buffer command_buffer) noexcept {
  if (!open_ || instance_ == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::submit_command_buffer(instance_, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_texture_to_buffer(
    webgpu_command_recorder recorder, webgpu_texture texture, webgpu_buffer buffer,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || texture == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_texture_to_buffer(instance_, recorder, texture, buffer, width, height,
                                             bytes_per_row);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_copy_buffer(webgpu_command_recorder recorder, webgpu_buffer source,
                                    webgpu_buffer destination,
                                    std::span<const webgpu_buffer_copy_region> regions) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0 ||
      regions.empty() || regions.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_buffer(instance_, recorder, source, destination, regions.data(),
                                  static_cast<std::uint32_t>(regions.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_buffer_to_texture_v2(
    webgpu_command_recorder recorder, webgpu_buffer source, webgpu_texture destination,
    const webgpu_texture_buffer_copy& region) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_buffer_to_texture_v2(instance_, recorder, source, destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_texture_to_buffer_v2(
    webgpu_command_recorder recorder, webgpu_texture source, webgpu_buffer destination,
    const webgpu_texture_buffer_copy& region) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_texture_to_buffer_v2(instance_, recorder, source, destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_copy_texture(webgpu_command_recorder recorder, webgpu_texture source,
                                     webgpu_texture destination,
                                     const webgpu_texture_copy_region& region) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_texture(instance_, recorder, source, destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_fill_buffer(webgpu_command_recorder recorder,
                                                  webgpu_buffer buffer, std::uint64_t offset,
                                                  std::uint64_t size,
                                                  std::uint32_t value) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || buffer == 0 || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_fill_buffer(instance_, recorder, buffer, offset, size, value);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_generate_mipmaps(webgpu_command_recorder recorder, webgpu_texture texture,
                                         const webgpu_texture_mipmap_range& range) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_generate_mipmaps(instance_, recorder, texture, &range);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::detail
