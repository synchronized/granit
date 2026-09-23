// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/renderer_state.h"
#include "backend/vulkan/renderer_state_utils.h"

#include "backend/vulkan/resources.h"
#include "backend/vulkan/result.h"
#include "backend/vulkan/surface.h"
#include "core/texture_format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <limits>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace granit::detail {

std::unique_ptr<backend_command_recorder_resource>
vulkan_renderer_state::allocate_command_recorder_resource() {
  return std::make_unique<vulkan_command_recorder_resource>(shared_from_this());
}

granit_result vulkan_renderer_state::create_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.initialize(device_));
}

granit_result vulkan_renderer_state::begin_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.begin(device_));
}

granit_result
vulkan_renderer_state::end_command_recorder(backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.end(device_));
}

granit_result vulkan_renderer_state::reset_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.reset(device_));
}

bool vulkan_renderer_state::command_recorder_is_recording(
    backend_command_recorder_resource& resource) noexcept {
  return static_cast<vulkan_command_recorder_resource&>(resource).native().state() ==
         command_recorder_state::recording;
}

granit_result vulkan_renderer_state::copy_buffer(
    backend_command_recorder_resource& recorder_resource, backend_buffer_resource& source,
    backend_buffer_resource& destination, std::span<const granit_buffer_copy_region> regions) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<VkBufferCopy> native_regions;
    native_regions.reserve(regions.size());
    for (const auto& region : regions) {
      native_regions.push_back({.srcOffset = region.source_offset,
                                .dstOffset = region.destination_offset,
                                .size = region.size});
    }
    return observe_device_result(recorder.copy_buffer(
        device_, static_cast<vulkan_buffer_resource&>(source).native().buffer,
        static_cast<vulkan_buffer_resource&>(destination).native().buffer, native_regions));
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result vulkan_renderer_state::copy_texture_to_buffer(
    backend_command_recorder_resource& recorder_resource, backend_texture_resource& source,
    backend_buffer_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const auto block = texture_format_block(format);
  VkBufferImageCopy copy{};
  copy.bufferOffset = layout.offset;
  copy.bufferRowLength =
      layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / block.bytes * block.width;
  copy.bufferImageHeight = layout.rows_per_image == 0 ? 0 : layout.rows_per_image * block.height;
  copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                           region.base_array_layer, region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  return observe_device_result(recorder.copy_texture_to_buffer(
      device_, static_cast<vulkan_texture_resource&>(source).native().image,
      static_cast<vulkan_buffer_resource&>(destination).native().buffer, copy));
}

granit_result vulkan_renderer_state::copy_buffer_to_texture(
    backend_command_recorder_resource& recorder_resource, backend_buffer_resource& source,
    backend_texture_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const auto block = texture_format_block(format);
  VkBufferImageCopy copy{};
  copy.bufferOffset = layout.offset;
  copy.bufferRowLength =
      layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / block.bytes * block.width;
  copy.bufferImageHeight = layout.rows_per_image == 0 ? 0 : layout.rows_per_image * block.height;
  copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                           region.base_array_layer, region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  return observe_device_result(recorder.copy_buffer_to_texture(
      device_, static_cast<vulkan_buffer_resource&>(source).native().buffer,
      static_cast<vulkan_texture_resource&>(destination).native().image, copy));
}

granit_result vulkan_renderer_state::copy_texture(
    backend_command_recorder_resource& recorder_resource, backend_texture_resource& source,
    backend_texture_resource& destination, const granit_texture_copy_region& region) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const VkImageCopy copy{
      .srcSubresource = {map_texture_aspect(region.aspect), region.source_mip_level,
                         region.source_base_array_layer, region.array_layer_count},
      .srcOffset = {static_cast<std::int32_t>(region.source_x),
                    static_cast<std::int32_t>(region.source_y),
                    static_cast<std::int32_t>(region.source_z)},
      .dstSubresource = {map_texture_aspect(region.aspect), region.destination_mip_level,
                         region.destination_base_array_layer, region.array_layer_count},
      .dstOffset = {static_cast<std::int32_t>(region.destination_x),
                    static_cast<std::int32_t>(region.destination_y),
                    static_cast<std::int32_t>(region.destination_z)},
      .extent = {region.width, region.height, region.depth}};
  return observe_device_result(recorder.copy_texture(
      device_, static_cast<vulkan_texture_resource&>(source).native().image,
      static_cast<vulkan_texture_resource&>(destination).native().image, copy));
}

granit_result vulkan_renderer_state::generate_mipmaps(
    backend_command_recorder_resource& recorder_resource, backend_texture_resource& texture,
    const granit_texture_desc& desc, const granit_texture_mipmap_range& range) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const VkExtent3D base_extent{std::max(UINT32_C(1), desc.width >> range.base_mip_level),
                               std::max(UINT32_C(1), desc.height >> range.base_mip_level),
                               std::max(UINT32_C(1), desc.depth >> range.base_mip_level)};
  return observe_device_result(recorder.generate_mipmaps(
      device_, static_cast<vulkan_texture_resource&>(texture).native().image, base_extent,
      range.base_mip_level, range.level_count, range.base_array_layer, range.array_layer_count));
}

granit_result
vulkan_renderer_state::fill_buffer(backend_command_recorder_resource& recorder_resource,
                                   backend_buffer_resource& buffer, std::uint64_t offset,
                                   std::uint64_t size, std::uint32_t value) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return observe_device_result(recorder.fill_buffer(
      device_, static_cast<vulkan_buffer_resource&>(buffer).native().buffer, offset, size, value));
}

granit_result vulkan_renderer_state::bind_graphics_pipeline(
    backend_command_recorder_resource& recorder_resource,
    backend_graphics_pipeline_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return recorder.bind_graphics_pipeline(
      device_, static_cast<vulkan_graphics_pipeline_resource&>(resource).native());
}

granit_result vulkan_renderer_state::bind_graphics_groups(
    backend_command_recorder_resource& recorder_resource,
    backend_pipeline_layout_resource& layout_resource, std::uint32_t first_group,
    std::span<backend_bind_group_resource* const> bind_groups,
    std::span<const std::uint32_t> dynamic_offsets,
    std::span<const backend_buffer_access> buffer_accesses,
    std::span<const backend_texture_access> texture_accesses) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<std::pair<VkBuffer, VkAccessFlags2>> native_buffers;
    std::vector<vulkan_image_access> native_textures;
    std::vector<VkDescriptorSet> native_groups;
    native_buffers.reserve(buffer_accesses.size());
    native_textures.reserve(texture_accesses.size());
    native_groups.reserve(bind_groups.size());
    for (auto* bind_group : bind_groups) {
      if (bind_group == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_groups.push_back(static_cast<vulkan_bind_group_resource&>(*bind_group).set());
    }
    for (const auto& access : buffer_accesses) {
      if (access.buffer == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto flags = access.type == backend_buffer_access_type::uniform_read
                             ? VkAccessFlags2{VK_ACCESS_2_UNIFORM_READ_BIT}
                             : VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
      native_buffers.emplace_back(
          static_cast<vulkan_buffer_resource&>(*access.buffer).native().buffer, flags);
    }
    for (const auto& access : texture_accesses) {
      if (access.texture == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const bool storage = access.type == backend_texture_access_type::storage_read_write;
      native_textures.push_back({
          .image = static_cast<vulkan_texture_resource&>(*access.texture).native().image,
          .range = {.aspectMask = access.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                      ? default_aspect(access.format)
                                      : map_texture_aspect(access.range.aspect),
                    .baseMipLevel = access.range.base_mip_level,
                    .levelCount = access.range.mip_level_count,
                    .baseArrayLayer = access.range.base_array_layer,
                    .layerCount = access.range.array_layer_count},
          .layout = storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .stages = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .access = storage ? VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                             VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                            : VkAccessFlags2{VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
          .preserve_content = false,
      });
    }
    const auto layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
    return recorder.bind_graphics_groups(device_, layout, first_group, native_groups,
                                         dynamic_offsets, native_buffers, native_textures);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
vulkan_renderer_state::bind_compute_pipeline(backend_command_recorder_resource& recorder_resource,
                                             backend_compute_pipeline_resource& resource) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost()
             ? GRANIT_ERROR_DEVICE_LOST
             : recorder.bind_compute_pipeline(
                   device_, static_cast<vulkan_compute_pipeline_resource&>(resource).native());
}

granit_result vulkan_renderer_state::bind_compute_groups(
    backend_command_recorder_resource& recorder_resource,
    backend_pipeline_layout_resource& layout_resource, std::uint32_t first_group,
    std::span<backend_bind_group_resource* const> bind_groups,
    std::span<const std::uint32_t> dynamic_offsets,
    std::span<const backend_buffer_access> buffer_accesses,
    std::span<const backend_texture_access> texture_accesses) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<std::pair<VkBuffer, VkAccessFlags2>> native_buffers;
    std::vector<vulkan_image_access> native_textures;
    std::vector<VkDescriptorSet> native_groups;
    native_buffers.reserve(buffer_accesses.size());
    native_textures.reserve(texture_accesses.size());
    native_groups.reserve(bind_groups.size());
    for (auto* bind_group : bind_groups) {
      if (bind_group == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_groups.push_back(static_cast<vulkan_bind_group_resource&>(*bind_group).set());
    }
    for (const auto& access : buffer_accesses) {
      if (access.buffer == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto flags = access.type == backend_buffer_access_type::uniform_read
                             ? VkAccessFlags2{VK_ACCESS_2_UNIFORM_READ_BIT}
                             : VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
      native_buffers.emplace_back(
          static_cast<vulkan_buffer_resource&>(*access.buffer).native().buffer, flags);
    }
    for (const auto& access : texture_accesses) {
      if (access.texture == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const bool storage = access.type == backend_texture_access_type::storage_read_write;
      native_textures.push_back({
          .image = static_cast<vulkan_texture_resource&>(*access.texture).native().image,
          .range = {.aspectMask = access.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                      ? default_aspect(access.format)
                                      : map_texture_aspect(access.range.aspect),
                    .baseMipLevel = access.range.base_mip_level,
                    .levelCount = access.range.mip_level_count,
                    .baseArrayLayer = access.range.base_array_layer,
                    .layerCount = access.range.array_layer_count},
          .layout = storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          .access = storage ? VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                             VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                            : VkAccessFlags2{VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
          .preserve_content = false,
      });
    }
    const auto layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
    return recorder.bind_compute_groups(device_, layout, first_group, native_groups,
                                        dynamic_offsets, native_buffers, native_textures);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result vulkan_renderer_state::dispatch(backend_command_recorder_resource& recorder_resource,
                                              std::uint32_t group_count_x,
                                              std::uint32_t group_count_y,
                                              std::uint32_t group_count_z) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.dispatch(device_, group_count_x, group_count_y, group_count_z);
}

granit_result
vulkan_renderer_state::set_viewports(backend_command_recorder_resource& recorder_resource,
                                     std::uint32_t first,
                                     std::span<const granit_viewport> viewports) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<VkViewport> native;
    native.reserve(viewports.size());
    for (const auto& value : viewports)
      native.push_back(map_viewport(value));
    return recorder.set_viewports(device_, first, native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
vulkan_renderer_state::set_scissors(backend_command_recorder_resource& recorder_resource,
                                    std::uint32_t first,
                                    std::span<const granit_scissor> scissors) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<VkRect2D> native;
    native.reserve(scissors.size());
    for (const auto& value : scissors)
      native.push_back({{value.x, value.y}, {value.width, value.height}});
    return recorder.set_scissors(device_, first, native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result vulkan_renderer_state::bind_vertex_buffers(
    backend_command_recorder_resource& recorder_resource, std::uint32_t first,
    std::span<backend_buffer_resource* const> buffers, std::span<const std::uint64_t> offsets) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<VkBuffer> native_buffers;
    std::vector<VkDeviceSize> native_offsets;
    native_buffers.reserve(buffers.size());
    native_offsets.reserve(offsets.size());
    for (auto* buffer : buffers) {
      if (buffer == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_buffers.push_back(static_cast<vulkan_buffer_resource&>(*buffer).native().buffer);
    }
    native_offsets.assign(offsets.begin(), offsets.end());
    return recorder.bind_vertex_buffers(device_, first, native_buffers, native_offsets);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
vulkan_renderer_state::bind_index_buffer(backend_command_recorder_resource& recorder_resource,
                                         backend_buffer_resource& buffer, std::uint64_t offset,
                                         granit_index_type type) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const auto native_type =
      type == GRANIT_INDEX_TYPE_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  return recorder.bind_index_buffer(
      device_, static_cast<vulkan_buffer_resource&>(buffer).native().buffer, offset, native_type);
}

granit_result vulkan_renderer_state::draw(backend_command_recorder_resource& recorder_resource,
                                          backend_texture_view_resource*,
                                          backend_graphics_pipeline_resource*,
                                          std::uint32_t vertex_count, std::uint32_t instance_count,
                                          std::uint32_t first_vertex,
                                          std::uint32_t first_instance) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost()
             ? GRANIT_ERROR_DEVICE_LOST
             : recorder.draw(device_, vertex_count, instance_count, first_vertex, first_instance);
}

granit_result vulkan_renderer_state::draw_indexed(
    backend_command_recorder_resource& recorder_resource, backend_texture_view_resource*,
    backend_graphics_pipeline_resource*, std::uint32_t index_count, std::uint32_t instance_count,
    std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.draw_indexed(device_, index_count, instance_count, first_index,
                                               vertex_offset, first_instance);
}

granit_result vulkan_renderer_state::begin_rendering(
    backend_command_recorder_resource& recorder_resource, granit_rendering_area area,
    std::span<const backend_color_attachment> color_attachments,
    const backend_depth_stencil_attachment* depth_stencil_attachment, std::uint32_t layer_count) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<VkRenderingAttachmentInfo> native_colors;
    std::vector<vulkan_image_access> image_accesses;
    native_colors.reserve(color_attachments.size());
    image_accesses.reserve(color_attachments.size() + (depth_stencil_attachment ? 1U : 0U));
    for (const auto& source : color_attachments) {
      if (source.texture == nullptr || source.view == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      VkRenderingAttachmentInfo target{};
      target.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      target.imageView = static_cast<vulkan_texture_view_resource&>(*source.view).native();
      target.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      target.loadOp = map_attachment_load(source.load_operation);
      target.storeOp = map_attachment_store(source.store_operation);
      target.clearValue.color = {{source.clear_value.red, source.clear_value.green,
                                  source.clear_value.blue, source.clear_value.alpha}};
      if (source.resolve_view != nullptr) {
        target.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        target.resolveImageView =
            static_cast<vulkan_texture_view_resource&>(*source.resolve_view).native();
        target.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      native_colors.push_back(target);
      image_accesses.push_back({
          .image = static_cast<vulkan_texture_resource&>(*source.texture).native().image,
          .range = {.aspectMask = source.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                      ? default_aspect(source.format)
                                      : map_texture_aspect(source.range.aspect),
                    .baseMipLevel = source.range.base_mip_level,
                    .levelCount = source.range.mip_level_count,
                    .baseArrayLayer = source.range.base_array_layer,
                    .layerCount = source.range.array_layer_count},
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
          .preserve_content = source.load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD,
      });
      if (source.resolve_texture != nullptr && source.resolve_view != nullptr) {
        image_accesses.push_back({
            .image = static_cast<vulkan_texture_resource&>(*source.resolve_texture).native().image,
            .range = {.aspectMask = source.resolve_range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                        ? default_aspect(source.format)
                                        : map_texture_aspect(source.resolve_range.aspect),
                      .baseMipLevel = source.resolve_range.base_mip_level,
                      .levelCount = source.resolve_range.mip_level_count,
                      .baseArrayLayer = source.resolve_range.base_array_layer,
                      .layerCount = source.resolve_range.array_layer_count},
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .preserve_content = false,
        });
      }
    }

    VkRenderingAttachmentInfo depth{}, stencil{};
    const VkRenderingAttachmentInfo *depth_ptr = nullptr, *stencil_ptr = nullptr;
    if (depth_stencil_attachment != nullptr) {
      const auto& source = *depth_stencil_attachment;
      if (source.texture == nullptr || source.view == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      depth.imageView = static_cast<vulkan_texture_view_resource&>(*source.view).native();
      depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depth.loadOp = map_attachment_load(source.depth_load_operation);
      depth.storeOp = map_attachment_store(source.depth_store_operation);
      depth.clearValue.depthStencil = {source.clear_value.depth, source.clear_value.stencil};
      depth_ptr = &depth;
      image_accesses.push_back({
          .image = static_cast<vulkan_texture_resource&>(*source.texture).native().image,
          .range = {.aspectMask = source.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                      ? default_aspect(source.format)
                                      : map_texture_aspect(source.range.aspect),
                    .baseMipLevel = source.range.base_mip_level,
                    .levelCount = source.range.mip_level_count,
                    .baseArrayLayer = source.range.base_array_layer,
                    .layerCount = source.range.array_layer_count},
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
          .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .preserve_content =
              source.depth_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD ||
              source.stencil_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD,
      });
      if (source.format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
          source.format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT) {
        stencil = depth;
        stencil.loadOp = map_attachment_load(source.stencil_load_operation);
        stencil.storeOp = map_attachment_store(source.stencil_store_operation);
        stencil_ptr = &stencil;
      }
    }
    const VkRect2D native_area{
        {static_cast<std::int32_t>(area.x), static_cast<std::int32_t>(area.y)},
        {area.width, area.height}};
    return observe_device_result(recorder.begin_rendering(
        device_, native_area, native_colors, depth_ptr, stencil_ptr, layer_count, image_accesses));
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result vulkan_renderer_state::end_rendering(
    backend_command_recorder_resource& recorder_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return observe_device_result(recorder.end_rendering(device_));
}

} // namespace granit::detail
