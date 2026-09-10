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

std::unique_ptr<backend_buffer_resource> vulkan_renderer_state::allocate_buffer_resource() {
  return std::make_unique<vulkan_buffer_resource>(shared_from_this());
}

std::unique_ptr<backend_texture_resource> vulkan_renderer_state::allocate_texture_resource() {
  return std::make_unique<vulkan_texture_resource>(shared_from_this());
}

std::unique_ptr<backend_texture_view_resource>
vulkan_renderer_state::allocate_texture_view_resource() {
  return std::make_unique<vulkan_texture_view_resource>(shared_from_this());
}

std::unique_ptr<backend_sampler_resource> vulkan_renderer_state::allocate_sampler_resource() {
  return std::make_unique<vulkan_sampler_resource>(shared_from_this());
}

std::unique_ptr<backend_shader_resource> vulkan_renderer_state::allocate_shader_resource() {
  return std::make_unique<vulkan_shader_resource>(shared_from_this());
}

std::unique_ptr<backend_bind_group_layout_resource>
vulkan_renderer_state::allocate_bind_group_layout_resource() {
  return std::make_unique<vulkan_bind_group_layout_resource>(shared_from_this());
}

std::unique_ptr<backend_bind_group_resource> vulkan_renderer_state::allocate_bind_group_resource() {
  return std::make_unique<vulkan_bind_group_resource>(shared_from_this());
}

granit_result
vulkan_renderer_state::create_native_buffer(const granit_buffer_desc& desc,
                                            backend_buffer_resource& buffer_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  VkBufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = desc.size;
  create_info.usage = map_buffer_usage(desc.usage);
  if (desc.memory_location == GRANIT_MEMORY_LOCATION_AUTOMATIC ||
      desc.memory_location == GRANIT_MEMORY_LOCATION_DEVICE) {
    create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  return observe_device_result(memory_allocator_.create_buffer(
      create_info, map_memory_location(desc.memory_location), buffer));
}

void vulkan_renderer_state::destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept {
  memory_allocator_.destroy_buffer(buffer);
}

void* vulkan_renderer_state::mapped_buffer_data(backend_buffer_resource& buffer) noexcept {
  return static_cast<vulkan_buffer_resource&>(buffer).native().mapped_data;
}

granit_result vulkan_renderer_state::flush_buffer(backend_buffer_resource& buffer_resource,
                                                  std::uint64_t offset,
                                                  std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  return observe_device_result(memory_allocator_.flush(buffer, offset, size));
}

granit_result vulkan_renderer_state::invalidate_buffer(backend_buffer_resource& buffer_resource,
                                                       std::uint64_t offset,
                                                       std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  return observe_device_result(memory_allocator_.invalidate(buffer, offset, size));
}

granit_result
vulkan_renderer_state::create_native_texture(const granit_texture_desc& desc,
                                             backend_texture_resource& texture_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& texture = static_cast<vulkan_texture_resource&>(texture_resource).native();
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  if (desc.dimension == GRANIT_TEXTURE_DIMENSION_CUBE)
    info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  info.format = map_texture_format(desc.format);
  info.extent = {desc.width, desc.height, desc.depth};
  info.mipLevels = desc.mip_levels;
  info.arrayLayers = desc.array_layers;
  info.samples = static_cast<VkSampleCountFlagBits>(desc.sample_count);
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = map_texture_usage(desc.usage);
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  return observe_device_result(
      memory_allocator_.create_image(info, map_memory_location(desc.memory_location), texture));
}

bool vulkan_renderer_state::texture_supports_linear_blit(
    granit_texture_format format) const noexcept {
  return physical_device_supports_linear_blit(instance_, device_, map_texture_format(format));
}

granit_result
vulkan_renderer_state::upload_texture(backend_texture_resource& resource,
                                      granit_texture_format format, const void* data,
                                      std::uint64_t size, const granit_texture_data_layout& layout,
                                      const granit_texture_write_region& region) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& texture = static_cast<vulkan_texture_resource&>(resource).native();
  const auto block = texture_format_block(format);
  if (block.bytes == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  VkBufferImageCopy copy{};
  copy.bufferRowLength =
      layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / block.bytes * block.width;
  copy.bufferImageHeight = layout.rows_per_image == 0 ? 0 : layout.rows_per_image * block.height;
  copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                           region.base_array_layer, region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  const auto slot_index = acquire_upload_slot(true);
  auto& context = *upload_slots_[slot_index].context;
  auto finish = [&](granit_result result) {
    release_upload_slot(slot_index);
    return observe_device_result(result);
  };
  auto result = context.ensure_capacity(memory_allocator_, size);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  std::memcpy(context.staging().mapped_data, data, static_cast<std::size_t>(size));
  result = memory_allocator_.flush(context.staging(), 0, size);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.begin(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  std::unique_lock queue_lock{queue_mutex_};
  const auto& functions = device_.functions();
  const vulkan_image_access destination{.image = texture.image,
                                        .range = {copy.imageSubresource.aspectMask,
                                                  copy.imageSubresource.mipLevel, 1,
                                                  copy.imageSubresource.baseArrayLayer, 1}};
  const auto previous = find_image_subresource(image_states_, destination);
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask =
      previous == image_states_.end() ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : previous->stages;
  barrier.srcAccessMask = previous == image_states_.end() ? 0 : previous->access;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  barrier.oldLayout =
      previous == image_states_.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture.image;
  barrier.subresourceRange = {copy.imageSubresource.aspectMask, copy.imageSubresource.mipLevel, 1,
                              copy.imageSubresource.baseArrayLayer,
                              copy.imageSubresource.layerCount};
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;
  functions.vkCmdPipelineBarrier2(context.command_buffer(), &dependency);
  functions.vkCmdCopyBufferToImage(context.command_buffer(), context.staging().buffer,
                                   texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  result = context.end(device_);
  if (result == GRANIT_SUCCESS)
    result = context.reset_fence(device_);
  if (result == GRANIT_SUCCESS) {
    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = context.command_buffer();
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    result = map_vulkan_result(
        functions.vkQueueSubmit2(device_.graphics_queue(), 1, &submit_info, context.fence()));
  }
  if (result == GRANIT_SUCCESS) {
    vulkan_image_access state{
        .image = texture.image,
        .range = {copy.imageSubresource.aspectMask, copy.imageSubresource.mipLevel, 1,
                  copy.imageSubresource.baseArrayLayer, copy.imageSubresource.layerCount},
        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .preserve_content = true};
    store_unit_image_accesses(image_states_, state);
  }
  queue_lock.unlock();
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(context.restore_signaled_fence(device_));
    return finish(result);
  }
  return finish(context.wait(device_));
}

void vulkan_renderer_state::destroy_native_texture(vulkan_image_allocation& texture) noexcept {
  {
    std::lock_guard lock{queue_mutex_};
    std::erase_if(image_states_, [&](const auto& state) { return state.image == texture.image; });
  }
  memory_allocator_.destroy_image(texture);
}

granit_result vulkan_renderer_state::create_native_texture_view(
    backend_texture_resource& texture_resource, const granit_texture_desc& texture_desc,
    const granit_texture_view_desc& view_desc,
    backend_texture_view_resource& view_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& texture = static_cast<vulkan_texture_resource&>(texture_resource).native();
  auto& view = static_cast<vulkan_texture_view_resource&>(view_resource).native();
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.image = texture.image;
  info.viewType = view_desc.dimension == GRANIT_TEXTURE_DIMENSION_CUBE ? VK_IMAGE_VIEW_TYPE_CUBE
                                                                       : VK_IMAGE_VIEW_TYPE_2D;
  info.format = map_texture_format(
      view_desc.format == GRANIT_TEXTURE_FORMAT_UNDEFINED ? texture_desc.format : view_desc.format);
  info.components = {.r = map_component_swizzle(view_desc.components.red),
                     .g = map_component_swizzle(view_desc.components.green),
                     .b = map_component_swizzle(view_desc.components.blue),
                     .a = map_component_swizzle(view_desc.components.alpha)};
  info.subresourceRange.aspectMask = view_desc.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                         ? default_aspect(texture_desc.format)
                                         : map_texture_aspect(view_desc.range.aspect);
  info.subresourceRange.baseMipLevel = view_desc.range.base_mip_level;
  info.subresourceRange.levelCount = view_desc.range.mip_level_count;
  info.subresourceRange.baseArrayLayer = view_desc.range.base_array_layer;
  info.subresourceRange.layerCount = view_desc.range.array_layer_count;
  return observe_device_result(map_vulkan_result(
      device_.functions().vkCreateImageView(device_.native_handle(), &info, nullptr, &view)));
}

void vulkan_renderer_state::destroy_native_texture_view(VkImageView view) noexcept {
  if (view != VK_NULL_HANDLE) {
    device_.functions().vkDestroyImageView(device_.native_handle(), view, nullptr);
  }
}

granit_result
vulkan_renderer_state::create_native_sampler(const granit_sampler_desc& desc,
                                             backend_sampler_resource& sampler_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& sampler = static_cast<vulkan_sampler_resource&>(sampler_resource).native();
  if ((desc.anisotropy_enabled != 0 && !device_.sampler_anisotropy_supported()) ||
      desc.max_anisotropy > device_.properties().limits.maxSamplerAnisotropy ||
      std::abs(desc.lod_bias) > device_.properties().limits.maxSamplerLodBias) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const auto map_filter = [](granit_filter value) {
    return value == GRANIT_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  };
  const auto map_mipmap = [](granit_mipmap_filter value) {
    return value == GRANIT_MIPMAP_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                                : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  };
  const auto map_address = [](granit_address_mode value) {
    switch (value) {
    case GRANIT_ADDRESS_MODE_MIRRORED_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    default:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
  };
  const VkCompareOp compare_ops[] = {VK_COMPARE_OP_NEVER,         VK_COMPARE_OP_NEVER,
                                     VK_COMPARE_OP_LESS,          VK_COMPARE_OP_EQUAL,
                                     VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER,
                                     VK_COMPARE_OP_NOT_EQUAL,     VK_COMPARE_OP_GREATER_OR_EQUAL,
                                     VK_COMPARE_OP_ALWAYS};
  VkSamplerCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.magFilter = map_filter(desc.mag_filter);
  info.minFilter = map_filter(desc.min_filter);
  info.mipmapMode = map_mipmap(desc.mipmap_filter);
  info.addressModeU = map_address(desc.address_mode_u);
  info.addressModeV = map_address(desc.address_mode_v);
  info.addressModeW = map_address(desc.address_mode_w);
  info.mipLodBias = desc.lod_bias;
  info.anisotropyEnable = desc.anisotropy_enabled != 0 ? VK_TRUE : VK_FALSE;
  info.maxAnisotropy = desc.max_anisotropy;
  info.compareEnable =
      desc.compare_operation != GRANIT_COMPARE_OPERATION_DISABLED ? VK_TRUE : VK_FALSE;
  info.compareOp = compare_ops[desc.compare_operation];
  info.minLod = desc.min_lod;
  info.maxLod = desc.max_lod;
  return observe_device_result(map_vulkan_result(
      device_.functions().vkCreateSampler(device_.native_handle(), &info, nullptr, &sampler)));
}

void vulkan_renderer_state::destroy_native_sampler(VkSampler sampler) noexcept {
  if (sampler != VK_NULL_HANDLE) {
    device_.functions().vkDestroySampler(device_.native_handle(), sampler, nullptr);
  }
}

granit_result
vulkan_renderer_state::create_native_shader(std::span<const std::uint32_t> code,
                                            backend_shader_resource& shader_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& shader = static_cast<vulkan_shader_resource&>(shader_resource).native();
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = code.size_bytes();
  info.pCode = code.data();
  return observe_device_result(map_vulkan_result(
      device_.functions().vkCreateShaderModule(device_.native_handle(), &info, nullptr, &shader)));
}

granit_result vulkan_renderer_state::create_shader(backend_shader_resource& shader,
                                                   granit_shader_stage,
                                                   granit_shader_code_format code_format,
                                                   std::span<const std::byte> code,
                                                   std::string_view) noexcept {
  if (code_format != GRANIT_SHADER_CODE_FORMAT_SPIRV)
    return GRANIT_ERROR_UNSUPPORTED;
  if (code.empty() || code.size() % sizeof(std::uint32_t) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<std::uint32_t> words(code.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), code.data(), code.size());
    return create_native_shader(words, shader);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void vulkan_renderer_state::destroy_native_shader(VkShaderModule shader) noexcept {
  if (shader != VK_NULL_HANDLE)
    device_.functions().vkDestroyShaderModule(device_.native_handle(), shader, nullptr);
}

granit_result vulkan_renderer_state::create_native_bind_group_layout(
    std::span<const granit_bind_group_layout_entry> entries,
    backend_bind_group_layout_resource& layout_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& layout = static_cast<vulkan_bind_group_layout_resource&>(layout_resource).native();
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(entries.size());
  for (const auto& entry : entries) {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    if (entry.type == GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
      type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    else if (entry.type == GRANIT_BINDING_TYPE_STORAGE_BUFFER)
      type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    else if (entry.type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE ||
             entry.type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE_CUBE ||
             entry.type == GRANIT_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE)
      type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    else if (entry.type == GRANIT_BINDING_TYPE_STORAGE_TEXTURE)
      type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    else if (entry.type == GRANIT_BINDING_TYPE_SAMPLER ||
             entry.type == GRANIT_BINDING_TYPE_COMPARISON_SAMPLER)
      type = VK_DESCRIPTOR_TYPE_SAMPLER;
    VkShaderStageFlags stages{};
    if ((entry.visibility & GRANIT_SHADER_STAGE_VERTEX_BIT) != 0)
      stages |= VK_SHADER_STAGE_VERTEX_BIT;
    if ((entry.visibility & GRANIT_SHADER_STAGE_FRAGMENT_BIT) != 0)
      stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if ((entry.visibility & GRANIT_SHADER_STAGE_COMPUTE_BIT) != 0)
      stages |= VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back({entry.binding, type, entry.array_count, stages, nullptr});
  }
  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = static_cast<std::uint32_t>(bindings.size());
  info.pBindings = bindings.data();
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateDescriptorSetLayout(
      device_.native_handle(), &info, nullptr, &layout)));
}

void vulkan_renderer_state::destroy_native_bind_group_layout(
    VkDescriptorSetLayout layout) noexcept {
  if (layout != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyDescriptorSetLayout(device_.native_handle(), layout, nullptr);
  }
}

granit_result vulkan_renderer_state::create_native_bind_group(
    backend_bind_group_layout_resource& layout_resource,
    std::span<const backend_bind_group_write> writes,
    backend_bind_group_resource& bind_group_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& layout = static_cast<vulkan_bind_group_layout_resource&>(layout_resource).native();
  auto& bind_group = static_cast<vulkan_bind_group_resource&>(bind_group_resource);
  auto& pool = bind_group.pool();
  auto& set = bind_group.set();
  const auto map_type = [](backend_binding_type type) {
    switch (type) {
    case backend_binding_type::dynamic_uniform_buffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case backend_binding_type::storage_buffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case backend_binding_type::sampled_texture:
    case backend_binding_type::sampled_texture_cube:
    case backend_binding_type::sampled_depth_texture:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case backend_binding_type::storage_texture:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case backend_binding_type::sampler:
    case backend_binding_type::comparison_sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case backend_binding_type::uniform_buffer:
    default:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
  };
  std::array<std::uint32_t, 6> counts{};
  for (const auto& write : writes) {
    std::size_t index{};
    if (write.type == backend_binding_type::dynamic_uniform_buffer)
      index = 1;
    else if (write.type == backend_binding_type::storage_buffer)
      index = 2;
    else if (write.type == backend_binding_type::sampled_texture ||
             write.type == backend_binding_type::sampled_texture_cube ||
             write.type == backend_binding_type::sampled_depth_texture)
      index = 3;
    else if (write.type == backend_binding_type::storage_texture)
      index = 4;
    else if (write.type == backend_binding_type::sampler ||
             write.type == backend_binding_type::comparison_sampler)
      index = 5;
    ++counts[index];
  }
  constexpr std::array types{
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_DESCRIPTOR_TYPE_SAMPLER};
  std::vector<VkDescriptorPoolSize> sizes;
  for (std::size_t index = 0; index < counts.size(); ++index) {
    if (counts[index] != 0)
      sizes.push_back({types[index], counts[index]});
  }
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
  pool_info.pPoolSizes = sizes.data();
  std::lock_guard lock{resource_mutex_};
  auto result = observe_device_result(map_vulkan_result(device_.functions().vkCreateDescriptorPool(
      device_.native_handle(), &pool_info, nullptr, &pool)));
  if (result != GRANIT_SUCCESS)
    return result;
  VkDescriptorSetAllocateInfo allocate{};
  allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate.descriptorPool = pool;
  allocate.descriptorSetCount = 1;
  allocate.pSetLayouts = &layout;
  result = observe_device_result(map_vulkan_result(
      device_.functions().vkAllocateDescriptorSets(device_.native_handle(), &allocate, &set)));
  if (result != GRANIT_SUCCESS) {
    device_.functions().vkDestroyDescriptorPool(device_.native_handle(), pool, nullptr);
    pool = VK_NULL_HANDLE;
    return result;
  }
  std::vector<VkDescriptorBufferInfo> buffers(writes.size());
  std::vector<VkDescriptorImageInfo> images(writes.size());
  std::vector<VkWriteDescriptorSet> native_writes(writes.size());
  for (std::size_t index = 0; index < writes.size(); ++index) {
    const auto& source = writes[index];
    auto& destination = native_writes[index];
    destination.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    destination.dstSet = set;
    destination.dstBinding = source.binding;
    destination.dstArrayElement = source.array_element;
    destination.descriptorCount = 1;
    destination.descriptorType = map_type(source.type);
    if (source.buffer != nullptr) {
      buffers[index] = {static_cast<vulkan_buffer_resource&>(*source.buffer).native().buffer,
                        source.offset, source.range};
      destination.pBufferInfo = &buffers[index];
    } else {
      if (source.texture_view != nullptr) {
        images[index].imageView =
            static_cast<vulkan_texture_view_resource&>(*source.texture_view).native();
      }
      if (source.sampler != nullptr)
        images[index].sampler = static_cast<vulkan_sampler_resource&>(*source.sampler).native();
      images[index].imageLayout = source.type == backend_binding_type::storage_texture
                                      ? VK_IMAGE_LAYOUT_GENERAL
                                      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      destination.pImageInfo = &images[index];
    }
  }
  device_.functions().vkUpdateDescriptorSets(device_.native_handle(),
                                             static_cast<std::uint32_t>(native_writes.size()),
                                             native_writes.data(), 0, nullptr);
  return GRANIT_SUCCESS;
}

void vulkan_renderer_state::destroy_native_bind_group(VkDescriptorPool pool) noexcept {
  if (pool != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyDescriptorPool(device_.native_handle(), pool, nullptr);
  }
}

} // namespace granit::detail
