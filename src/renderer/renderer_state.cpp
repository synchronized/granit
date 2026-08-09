// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

#include "backend/vulkan/result.h"
#include "backend/vulkan/surface.h"

#include <cmath>
#include <cstring>

namespace granit::detail {
namespace {

VkBufferUsageFlags map_buffer_usage(granit_buffer_usage usage) noexcept {
  VkBufferUsageFlags flags{};
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_INDEX_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_UNIFORM_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_STORAGE_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_INDIRECT_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  return flags;
}

vulkan_memory_location map_memory_location(granit_memory_location location) noexcept {
  switch (location) {
  case GRANIT_MEMORY_LOCATION_DEVICE:
    return vulkan_memory_location::device;
  case GRANIT_MEMORY_LOCATION_UPLOAD:
    return vulkan_memory_location::upload;
  case GRANIT_MEMORY_LOCATION_READBACK:
    return vulkan_memory_location::readback;
  default:
    return vulkan_memory_location::automatic;
  }
}

VkFormat map_texture_format(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
    return VK_FORMAT_R8G8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case GRANIT_TEXTURE_FORMAT_BGRA8_UNORM:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case GRANIT_TEXTURE_FORMAT_BGRA8_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case GRANIT_TEXTURE_FORMAT_D16_UNORM:
    return VK_FORMAT_D16_UNORM;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    return VK_FORMAT_D32_SFLOAT;
  case GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT:
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

VkImageAspectFlags default_aspect(granit_texture_format format) noexcept {
  if (format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
      format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT) {
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  return format >= GRANIT_TEXTURE_FORMAT_D16_UNORM ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                   : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageAspectFlags map_texture_aspect(granit_texture_aspect aspect) noexcept {
  VkImageAspectFlags flags{};
  if ((aspect & GRANIT_TEXTURE_ASPECT_COLOR_BIT) != 0)
    flags |= VK_IMAGE_ASPECT_COLOR_BIT;
  if ((aspect & GRANIT_TEXTURE_ASPECT_DEPTH_BIT) != 0)
    flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
  if ((aspect & GRANIT_TEXTURE_ASPECT_STENCIL_BIT) != 0)
    flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  return flags;
}

VkImageUsageFlags map_texture_usage(granit_texture_usage usage) noexcept {
  VkImageUsageFlags flags{};
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) != 0)
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) != 0)
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_SAMPLED_BIT) != 0)
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_STORAGE_BIT) != 0)
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  return flags;
}

} // namespace

granit_result renderer_state::initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types) {
  validation_enabled_ = enable_validation;
  const auto instance_result = instance_.initialize({.application_name = application_name,
                                                     .enable_validation = enable_validation,
                                                     .surface_types = surface_types});
  if (instance_result != GRANIT_SUCCESS) {
    return instance_result;
  }

  const auto device_result = device_.initialize(instance_, surface_types);
  if (device_result != GRANIT_SUCCESS) {
    instance_.reset();
    return device_result;
  }

  const auto allocator_result = memory_allocator_.initialize(instance_, device_);
  if (allocator_result != GRANIT_SUCCESS) {
    device_.reset();
    instance_.reset();
    return allocator_result;
  }
  surface_types_ = surface_types;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::create_win32_surface(void* native_instance, void* native_window,
                                                   VkSurfaceKHR& surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return detail::create_win32_surface(instance_, device_, native_instance, native_window, surface);
}

void renderer_state::destroy_native_surface(VkSurfaceKHR surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  detail::destroy_surface(instance_, surface);
}

granit_result renderer_state::create_swapchain(VkSurfaceKHR surface,
                                               const vulkan_swapchain_desc& desc,
                                               vulkan_swapchain& swapchain) {
  std::lock_guard lock{resource_mutex_};
  return swapchain.initialize(instance_, device_, surface, desc);
}

granit_result renderer_state::recreate_swapchain(VkSurfaceKHR surface,
                                                 const vulkan_swapchain_desc& desc,
                                                 vulkan_swapchain& swapchain) {
  std::lock_guard lock{resource_mutex_};
  return swapchain.recreate(instance_, device_, surface, desc);
}

vulkan_swapchain_info
renderer_state::get_swapchain_info(const vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  return swapchain.info();
}

void renderer_state::destroy_native_swapchain(vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  swapchain.reset(device_);
}

granit_result renderer_state::create_native_buffer(const granit_buffer_desc& desc,
                                                   vulkan_buffer_allocation& buffer) noexcept {
  VkBufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = desc.size;
  create_info.usage = map_buffer_usage(desc.usage);
  if (desc.memory_location == GRANIT_MEMORY_LOCATION_AUTOMATIC ||
      desc.memory_location == GRANIT_MEMORY_LOCATION_DEVICE) {
    create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  return memory_allocator_.create_buffer(create_info, map_memory_location(desc.memory_location),
                                         buffer);
}

void renderer_state::destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept {
  memory_allocator_.destroy_buffer(buffer);
}

granit_result renderer_state::flush_buffer(const vulkan_buffer_allocation& buffer,
                                           VkDeviceSize offset, VkDeviceSize size) noexcept {
  return memory_allocator_.flush(buffer, offset, size);
}

granit_result renderer_state::invalidate_buffer(const vulkan_buffer_allocation& buffer,
                                                VkDeviceSize offset, VkDeviceSize size) noexcept {
  return memory_allocator_.invalidate(buffer, offset, size);
}

granit_result renderer_state::upload_buffer(const vulkan_buffer_allocation& buffer,
                                            VkDeviceSize offset, const void* data,
                                            VkDeviceSize size) noexcept {
  vulkan_buffer_allocation staging;
  VkBufferCreateInfo staging_info{};
  staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_info.size = size;
  staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  auto result =
      memory_allocator_.create_buffer(staging_info, vulkan_memory_location::upload, staging);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  std::memcpy(staging.mapped_data, data, static_cast<std::size_t>(size));
  result = memory_allocator_.flush(staging, 0, size);
  if (result != GRANIT_SUCCESS) {
    memory_allocator_.destroy_buffer(staging);
    return result;
  }

  std::lock_guard queue_lock{queue_mutex_};
  const auto& functions = device_.functions();
  VkCommandPool pool{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  VkFence fence{VK_NULL_HANDLE};

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex = device_.graphics_queue_family();
  auto vk_result =
      functions.vkCreateCommandPool(device_.native_handle(), &pool_info, nullptr, &pool);
  if (vk_result == VK_SUCCESS) {
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    vk_result = functions.vkAllocateCommandBuffers(device_.native_handle(), &allocate_info,
                                                   &command_buffer);
  }
  if (vk_result == VK_SUCCESS) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_result = functions.vkBeginCommandBuffer(command_buffer, &begin_info);
  }
  if (vk_result == VK_SUCCESS) {
    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = offset, .size = size};
    functions.vkCmdCopyBuffer(command_buffer, staging.buffer, buffer.buffer, 1, &copy);
    vk_result = functions.vkEndCommandBuffer(command_buffer);
  }
  if (vk_result == VK_SUCCESS) {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vk_result = functions.vkCreateFence(device_.native_handle(), &fence_info, nullptr, &fence);
  }
  if (vk_result == VK_SUCCESS) {
    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = command_buffer;
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    vk_result = functions.vkQueueSubmit2(device_.graphics_queue(), 1, &submit_info, fence);
  }
  if (vk_result == VK_SUCCESS) {
    vk_result = functions.vkWaitForFences(device_.native_handle(), 1, &fence, VK_TRUE, UINT64_MAX);
  }

  if (fence != VK_NULL_HANDLE) {
    functions.vkDestroyFence(device_.native_handle(), fence, nullptr);
  }
  if (pool != VK_NULL_HANDLE) {
    functions.vkDestroyCommandPool(device_.native_handle(), pool, nullptr);
  }
  memory_allocator_.destroy_buffer(staging);
  return map_vulkan_result(vk_result);
}

granit_result renderer_state::create_native_texture(const granit_texture_desc& desc,
                                                    vulkan_image_allocation& texture) noexcept {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = map_texture_format(desc.format);
  info.extent = {desc.width, desc.height, desc.depth};
  info.mipLevels = desc.mip_levels;
  info.arrayLayers = desc.array_layers;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = map_texture_usage(desc.usage);
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  return memory_allocator_.create_image(info, map_memory_location(desc.memory_location), texture);
}

void renderer_state::destroy_native_texture(vulkan_image_allocation& texture) noexcept {
  memory_allocator_.destroy_image(texture);
}

granit_result renderer_state::create_native_texture_view(const vulkan_image_allocation& texture,
                                                         const granit_texture_desc& texture_desc,
                                                         const granit_texture_view_desc& view_desc,
                                                         VkImageView& view) noexcept {
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.image = texture.image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = map_texture_format(
      view_desc.format == GRANIT_TEXTURE_FORMAT_UNDEFINED ? texture_desc.format : view_desc.format);
  info.subresourceRange.aspectMask = view_desc.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC
                                         ? default_aspect(texture_desc.format)
                                         : map_texture_aspect(view_desc.range.aspect);
  info.subresourceRange.baseMipLevel = view_desc.range.base_mip_level;
  info.subresourceRange.levelCount = view_desc.range.mip_level_count;
  info.subresourceRange.baseArrayLayer = view_desc.range.base_array_layer;
  info.subresourceRange.layerCount = view_desc.range.array_layer_count;
  return map_vulkan_result(
      device_.functions().vkCreateImageView(device_.native_handle(), &info, nullptr, &view));
}

void renderer_state::destroy_native_texture_view(VkImageView view) noexcept {
  if (view != VK_NULL_HANDLE) {
    device_.functions().vkDestroyImageView(device_.native_handle(), view, nullptr);
  }
}

granit_result renderer_state::create_native_sampler(const granit_sampler_desc& desc,
                                                    VkSampler& sampler) noexcept {
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
  return map_vulkan_result(
      device_.functions().vkCreateSampler(device_.native_handle(), &info, nullptr, &sampler));
}

void renderer_state::destroy_native_sampler(VkSampler sampler) noexcept {
  if (sampler != VK_NULL_HANDLE) {
    device_.functions().vkDestroySampler(device_.native_handle(), sampler, nullptr);
  }
}

granit_result
renderer_state::create_native_command_recorder(vulkan_command_recorder& recorder) noexcept {
  return recorder.initialize(device_);
}

granit_result renderer_state::begin_command_recorder(vulkan_command_recorder& recorder) noexcept {
  return recorder.begin(device_);
}

granit_result renderer_state::end_command_recorder(vulkan_command_recorder& recorder) noexcept {
  return recorder.end(device_);
}

granit_result renderer_state::reset_command_recorder(vulkan_command_recorder& recorder) noexcept {
  return recorder.reset(device_);
}

granit_result renderer_state::copy_buffer(vulkan_command_recorder& recorder, VkBuffer source,
                                          VkBuffer destination,
                                          std::span<const VkBufferCopy> regions) noexcept {
  return recorder.copy_buffer(device_, source, destination, regions);
}

granit_result renderer_state::fill_buffer(vulkan_command_recorder& recorder, VkBuffer buffer,
                                          VkDeviceSize offset, VkDeviceSize size,
                                          std::uint32_t value) noexcept {
  return recorder.fill_buffer(device_, buffer, offset, size, value);
}

granit_result
renderer_state::begin_rendering(vulkan_command_recorder& recorder, VkRect2D area,
                                std::span<const VkRenderingAttachmentInfo> color_attachments,
                                const VkRenderingAttachmentInfo* depth_attachment,
                                const VkRenderingAttachmentInfo* stencil_attachment,
                                std::uint32_t layer_count) noexcept {
  return recorder.begin_rendering(device_, area, color_attachments, depth_attachment,
                                  stencil_attachment, layer_count);
}

granit_result renderer_state::end_rendering(vulkan_command_recorder& recorder) noexcept {
  return recorder.end_rendering(device_);
}

void renderer_state::destroy_native_command_recorder(vulkan_command_recorder& recorder) noexcept {
  recorder.destroy(device_);
}

} // namespace granit::detail
