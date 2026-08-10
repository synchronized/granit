// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

#include "backend/vulkan/result.h"
#include "backend/vulkan/surface.h"

#include <algorithm>
#include <array>
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

VkFormat map_vertex_format(granit_vertex_format format) noexcept {
  switch (format) {
  case GRANIT_VERTEX_FORMAT_FLOAT32:
    return VK_FORMAT_R32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_FLOAT32X2:
    return VK_FORMAT_R32G32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_FLOAT32X3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_FLOAT32X4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_UINT32:
    return VK_FORMAT_R32_UINT;
  case GRANIT_VERTEX_FORMAT_UINT32X2:
    return VK_FORMAT_R32G32_UINT;
  case GRANIT_VERTEX_FORMAT_UINT32X3:
    return VK_FORMAT_R32G32B32_UINT;
  case GRANIT_VERTEX_FORMAT_UINT32X4:
    return VK_FORMAT_R32G32B32A32_UINT;
  case GRANIT_VERTEX_FORMAT_SINT32:
    return VK_FORMAT_R32_SINT;
  case GRANIT_VERTEX_FORMAT_SINT32X2:
    return VK_FORMAT_R32G32_SINT;
  case GRANIT_VERTEX_FORMAT_SINT32X3:
    return VK_FORMAT_R32G32B32_SINT;
  case GRANIT_VERTEX_FORMAT_SINT32X4:
    return VK_FORMAT_R32G32B32A32_SINT;
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

renderer_state::~renderer_state() {
  static_cast<void>(wait_for_all_submissions());
  for (auto& slot : frame_slots_) {
    slot.postamble->destroy(device_);
    slot.preamble->destroy(device_);
    slot.context->destroy(device_);
  }
  frame_slots_.clear();
}

granit_result renderer_state::initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types,
                                         std::uint32_t frames_in_flight) {
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
  try {
    frame_slots_.reserve(frames_in_flight);
    for (std::uint32_t index = 0; index < frames_in_flight; ++index) {
      frame_slot slot{.context = std::make_unique<vulkan_frame_context>(),
                      .preamble = std::make_unique<vulkan_command_recorder>(),
                      .postamble = std::make_unique<vulkan_command_recorder>()};
      const auto frame_result = slot.context->initialize(device_);
      if (frame_result != GRANIT_SUCCESS) {
        for (auto& initialized : frame_slots_) {
          initialized.preamble->destroy(device_);
          initialized.postamble->destroy(device_);
          initialized.context->destroy(device_);
        }
        frame_slots_.clear();
        memory_allocator_.reset();
        device_.reset();
        instance_.reset();
        return frame_result;
      }
      const auto preamble_result = slot.preamble->initialize(device_);
      if (preamble_result != GRANIT_SUCCESS) {
        slot.context->destroy(device_);
        for (auto& initialized : frame_slots_) {
          initialized.preamble->destroy(device_);
          initialized.context->destroy(device_);
        }
        frame_slots_.clear();
        memory_allocator_.reset();
        device_.reset();
        instance_.reset();
        return preamble_result;
      }
      const auto postamble_result = slot.postamble->initialize(device_);
      if (postamble_result != GRANIT_SUCCESS) {
        slot.preamble->destroy(device_);
        slot.context->destroy(device_);
        for (auto& initialized : frame_slots_) {
          initialized.postamble->destroy(device_);
          initialized.preamble->destroy(device_);
          initialized.context->destroy(device_);
        }
        frame_slots_.clear();
        memory_allocator_.reset();
        device_.reset();
        instance_.reset();
        return postamble_result;
      }
      frame_slots_.push_back(std::move(slot));
    }
  } catch (...) {
    for (auto& slot : frame_slots_) {
      slot.postamble->destroy(device_);
      slot.preamble->destroy(device_);
      slot.context->destroy(device_);
    }
    frame_slots_.clear();
    memory_allocator_.reset();
    device_.reset();
    instance_.reset();
    throw;
  }
  surface_types_ = surface_types;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::create_win32_surface(void* native_instance, void* native_window,
                                                   VkSurfaceKHR& surface) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return observe_device_result(
      detail::create_win32_surface(instance_, device_, native_instance, native_window, surface));
}

void renderer_state::destroy_native_surface(VkSurfaceKHR surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  detail::destroy_surface(instance_, surface);
}

granit_result renderer_state::create_swapchain(VkSurfaceKHR surface,
                                               const vulkan_swapchain_desc& desc,
                                               vulkan_swapchain& swapchain) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(swapchain.initialize(instance_, device_, surface, desc));
}

granit_result renderer_state::recreate_swapchain(VkSurfaceKHR surface,
                                                 const vulkan_swapchain_desc& desc,
                                                 vulkan_swapchain& swapchain) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  {
    std::lock_guard queue_lock{queue_mutex_};
    for (const auto image : swapchain.images()) {
      std::erase_if(image_states_, [&](const auto& state) { return state.image == image; });
    }
  }
  return observe_device_result(swapchain.recreate(instance_, device_, surface, desc));
}

vulkan_swapchain_info
renderer_state::get_swapchain_info(const vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  return swapchain.info();
}

void renderer_state::destroy_native_swapchain(vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  {
    std::lock_guard queue_lock{queue_mutex_};
    for (const auto image : swapchain.images()) {
      std::erase_if(image_states_, [&](const auto& state) { return state.image == image; });
    }
  }
  swapchain.reset(device_);
}

granit_result renderer_state::create_native_buffer(const granit_buffer_desc& desc,
                                                   vulkan_buffer_allocation& buffer) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
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

void renderer_state::destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept {
  memory_allocator_.destroy_buffer(buffer);
}

granit_result renderer_state::flush_buffer(const vulkan_buffer_allocation& buffer,
                                           VkDeviceSize offset, VkDeviceSize size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(memory_allocator_.flush(buffer, offset, size));
}

granit_result renderer_state::invalidate_buffer(const vulkan_buffer_allocation& buffer,
                                                VkDeviceSize offset, VkDeviceSize size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(memory_allocator_.invalidate(buffer, offset, size));
}

granit_result renderer_state::upload_buffer(const vulkan_buffer_allocation& buffer,
                                            VkDeviceSize offset, const void* data,
                                            VkDeviceSize size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  vulkan_buffer_allocation staging;
  VkBufferCreateInfo staging_info{};
  staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_info.size = size;
  staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  auto result =
      memory_allocator_.create_buffer(staging_info, vulkan_memory_location::upload, staging);
  if (result != GRANIT_SUCCESS) {
    return observe_device_result(result);
  }
  std::memcpy(staging.mapped_data, data, static_cast<std::size_t>(size));
  result = memory_allocator_.flush(staging, 0, size);
  if (result != GRANIT_SUCCESS) {
    memory_allocator_.destroy_buffer(staging);
    return observe_device_result(result);
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
  return observe_device_result(map_vulkan_result(vk_result));
}

granit_result renderer_state::create_native_texture(const granit_texture_desc& desc,
                                                    vulkan_image_allocation& texture) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
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
  return observe_device_result(
      memory_allocator_.create_image(info, map_memory_location(desc.memory_location), texture));
}

void renderer_state::destroy_native_texture(vulkan_image_allocation& texture) noexcept {
  {
    std::lock_guard lock{queue_mutex_};
    std::erase_if(image_states_, [&](const auto& state) { return state.image == texture.image; });
  }
  memory_allocator_.destroy_image(texture);
}

granit_result renderer_state::create_native_texture_view(const vulkan_image_allocation& texture,
                                                         const granit_texture_desc& texture_desc,
                                                         const granit_texture_view_desc& view_desc,
                                                         VkImageView& view) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
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
  return observe_device_result(map_vulkan_result(
      device_.functions().vkCreateImageView(device_.native_handle(), &info, nullptr, &view)));
}

void renderer_state::destroy_native_texture_view(VkImageView view) noexcept {
  if (view != VK_NULL_HANDLE) {
    device_.functions().vkDestroyImageView(device_.native_handle(), view, nullptr);
  }
}

granit_result renderer_state::create_native_sampler(const granit_sampler_desc& desc,
                                                    VkSampler& sampler) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
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

void renderer_state::destroy_native_sampler(VkSampler sampler) noexcept {
  if (sampler != VK_NULL_HANDLE) {
    device_.functions().vkDestroySampler(device_.native_handle(), sampler, nullptr);
  }
}

granit_result renderer_state::create_native_shader(std::span<const std::uint32_t> code,
                                                   VkShaderModule& shader) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = code.size_bytes();
  info.pCode = code.data();
  return observe_device_result(map_vulkan_result(
      device_.functions().vkCreateShaderModule(device_.native_handle(), &info, nullptr, &shader)));
}

void renderer_state::destroy_native_shader(VkShaderModule shader) noexcept {
  if (shader != VK_NULL_HANDLE)
    device_.functions().vkDestroyShaderModule(device_.native_handle(), shader, nullptr);
}

granit_result renderer_state::create_native_bind_group_layout(
    std::span<const granit_bind_group_layout_entry> entries,
    VkDescriptorSetLayout& layout) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(entries.size());
  for (const auto& entry : entries) {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    if (entry.type == GRANIT_BINDING_TYPE_STORAGE_BUFFER)
      type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    else if (entry.type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE)
      type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    else if (entry.type == GRANIT_BINDING_TYPE_STORAGE_TEXTURE)
      type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    else if (entry.type == GRANIT_BINDING_TYPE_SAMPLER)
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

void renderer_state::destroy_native_bind_group_layout(VkDescriptorSetLayout layout) noexcept {
  if (layout != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyDescriptorSetLayout(device_.native_handle(), layout, nullptr);
  }
}

granit_result
renderer_state::create_native_bind_group(VkDescriptorSetLayout layout,
                                         std::span<const vulkan_bind_group_write> writes,
                                         VkDescriptorPool& pool, VkDescriptorSet& set) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::array<std::uint32_t, 5> counts{};
  for (const auto& write : writes) {
    std::size_t index{};
    if (write.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      index = 1;
    else if (write.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
      index = 2;
    else if (write.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
      index = 3;
    else if (write.type == VK_DESCRIPTOR_TYPE_SAMPLER)
      index = 4;
    ++counts[index];
  }
  constexpr std::array types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                             VK_DESCRIPTOR_TYPE_SAMPLER};
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
    destination.descriptorType = source.type;
    if (source.buffer != VK_NULL_HANDLE) {
      buffers[index] = {source.buffer, source.offset, source.range};
      destination.pBufferInfo = &buffers[index];
    } else {
      images[index].imageView = source.image_view;
      images[index].sampler = source.sampler;
      images[index].imageLayout = source.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
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

void renderer_state::destroy_native_bind_group(VkDescriptorPool pool) noexcept {
  if (pool != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyDescriptorPool(device_.native_handle(), pool, nullptr);
  }
}

granit_result renderer_state::create_native_pipeline_layout(
    std::span<const VkDescriptorSetLayout> bind_group_layouts, VkPipelineLayout& layout) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = static_cast<std::uint32_t>(bind_group_layouts.size());
  info.pSetLayouts = bind_group_layouts.data();
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreatePipelineLayout(
      device_.native_handle(), &info, nullptr, &layout)));
}

void renderer_state::destroy_native_pipeline_layout(VkPipelineLayout layout) noexcept {
  if (layout != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipelineLayout(device_.native_handle(), layout, nullptr);
  }
}

granit_result renderer_state::create_native_graphics_pipeline(
    VkPipelineLayout layout, VkShaderModule vertex_shader, const char* vertex_entry,
    VkShaderModule fragment_shader, const char* fragment_entry,
    std::span<const granit_vertex_buffer_layout> vertex_buffers, granit_primitive_state primitive,
    granit_depth_state depth_state, std::span<const granit_color_blend_state> color_blends,
    std::span<const granit_texture_format> color_formats,
    granit_texture_format depth_stencil_format, granit_sample_count sample_count,
    VkPipeline& pipeline) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertex_shader;
  stages[0].pName = vertex_entry;
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragment_shader;
  stages[1].pName = fragment_entry;

  const auto& limits = device_.properties().limits;
  std::size_t attribute_count{};
  for (const auto& buffer : vertex_buffers)
    attribute_count += buffer.attribute_count;
  if (vertex_buffers.size() > limits.maxVertexInputBindings ||
      attribute_count > limits.maxVertexInputAttributes)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;
  bindings.reserve(vertex_buffers.size());
  attributes.reserve(attribute_count);
  for (std::uint32_t binding = 0; binding < vertex_buffers.size(); ++binding) {
    const auto& source = vertex_buffers[binding];
    if (source.stride > limits.maxVertexInputBindingStride)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    bindings.push_back({.binding = binding,
                        .stride = source.stride,
                        .inputRate = source.step_mode == GRANIT_VERTEX_STEP_MODE_INSTANCE
                                         ? VK_VERTEX_INPUT_RATE_INSTANCE
                                         : VK_VERTEX_INPUT_RATE_VERTEX});
    for (std::uint32_t index = 0; index < source.attribute_count; ++index) {
      const auto& attribute = source.attributes[index];
      if (attribute.offset > limits.maxVertexInputAttributeOffset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      attributes.push_back({.location = attribute.location,
                            .binding = binding,
                            .format = map_vertex_format(attribute.format),
                            .offset = attribute.offset});
    }
  }
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
  vertex_input.pVertexBindingDescriptions = bindings.data();
  vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
  vertex_input.pVertexAttributeDescriptions = attributes.data();
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  constexpr std::array topologies{VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP};
  input_assembly.topology = topologies[primitive.topology - GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST];
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rasterization{};
  rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  constexpr std::array polygon_modes{VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE,
                                     VK_POLYGON_MODE_POINT};
  constexpr std::array cull_modes{VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT,
                                  VK_CULL_MODE_FRONT_AND_BACK};
  constexpr std::array front_faces{VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FRONT_FACE_CLOCKWISE};
  if (primitive.polygon_mode != GRANIT_POLYGON_MODE_FILL &&
      !device_.fill_mode_non_solid_supported())
    return GRANIT_ERROR_UNSUPPORTED;
  rasterization.polygonMode = polygon_modes[primitive.polygon_mode - GRANIT_POLYGON_MODE_FILL];
  rasterization.cullMode = cull_modes[primitive.cull_mode - GRANIT_CULL_MODE_NONE];
  rasterization.frontFace = front_faces[primitive.front_face - GRANIT_FRONT_FACE_COUNTER_CLOCKWISE];
  rasterization.lineWidth = 1.0F;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(sample_count);
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  constexpr std::array compare_operations{VK_COMPARE_OP_NEVER,
                                          VK_COMPARE_OP_LESS,
                                          VK_COMPARE_OP_EQUAL,
                                          VK_COMPARE_OP_LESS_OR_EQUAL,
                                          VK_COMPARE_OP_GREATER,
                                          VK_COMPARE_OP_NOT_EQUAL,
                                          VK_COMPARE_OP_GREATER_OR_EQUAL,
                                          VK_COMPARE_OP_ALWAYS};
  depth.depthTestEnable = depth_state.test_enabled != 0;
  depth.depthWriteEnable = depth_state.write_enabled != 0;
  depth.depthCompareOp = compare_operations[depth_state.compare - GRANIT_COMPARE_OPERATION_NEVER];
  std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(color_formats.size());
  constexpr std::array blend_factors{
      VK_BLEND_FACTOR_ZERO,      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
      VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA};
  constexpr std::array blend_operations{VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT,
                                        VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN,
                                        VK_BLEND_OP_MAX};
  for (std::size_t index = 0; index < blend_attachments.size(); ++index) {
    auto& target = blend_attachments[index];
    const auto& source = color_blends[index];
    target.blendEnable = source.enabled != 0;
    target.srcColorBlendFactor =
        blend_factors[source.source_color_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.dstColorBlendFactor =
        blend_factors[source.destination_color_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.colorBlendOp = blend_operations[source.color_operation - GRANIT_BLEND_OPERATION_ADD];
    target.srcAlphaBlendFactor =
        blend_factors[source.source_alpha_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.dstAlphaBlendFactor =
        blend_factors[source.destination_alpha_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.alphaBlendOp = blend_operations[source.alpha_operation - GRANIT_BLEND_OPERATION_ADD];
    target.colorWriteMask = 0;
    if ((source.write_mask & GRANIT_COLOR_WRITE_RED_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    if ((source.write_mask & GRANIT_COLOR_WRITE_GREEN_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    if ((source.write_mask & GRANIT_COLOR_WRITE_BLUE_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    if ((source.write_mask & GRANIT_COLOR_WRITE_ALPHA_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
  }
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = static_cast<std::uint32_t>(blend_attachments.size());
  blend.pAttachments = blend_attachments.data();
  const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
  dynamic.pDynamicStates = dynamic_states.data();
  std::vector<VkFormat> native_formats;
  native_formats.reserve(color_formats.size());
  for (const auto format : color_formats)
    native_formats.push_back(map_texture_format(format));
  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = static_cast<std::uint32_t>(native_formats.size());
  rendering.pColorAttachmentFormats = native_formats.data();
  const auto depth_format = map_texture_format(depth_stencil_format);
  rendering.depthAttachmentFormat = depth_format;
  if (depth_stencil_format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
      depth_stencil_format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT)
    rendering.stencilAttachmentFormat = depth_format;

  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = &rendering;
  info.stageCount = static_cast<std::uint32_t>(stages.size());
  info.pStages = stages.data();
  info.pVertexInputState = &vertex_input;
  info.pInputAssemblyState = &input_assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &rasterization;
  info.pMultisampleState = &multisample;
  info.pDepthStencilState = &depth;
  info.pColorBlendState = &blend;
  info.pDynamicState = &dynamic;
  info.layout = layout;
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateGraphicsPipelines(
      device_.native_handle(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline)));
}

void renderer_state::destroy_native_graphics_pipeline(VkPipeline pipeline) noexcept {
  if (pipeline != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipeline(device_.native_handle(), pipeline, nullptr);
  }
}

granit_result renderer_state::create_native_compute_pipeline(VkPipelineLayout layout,
                                                             VkShaderModule compute_shader,
                                                             const char* compute_entry,
                                                             VkPipeline& pipeline) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  VkComputePipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  info.stage.module = compute_shader;
  info.stage.pName = compute_entry;
  info.layout = layout;
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateComputePipelines(
      device_.native_handle(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline)));
}

void renderer_state::destroy_native_compute_pipeline(VkPipeline pipeline) noexcept {
  if (pipeline != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipeline(device_.native_handle(), pipeline, nullptr);
  }
}

granit_result
renderer_state::create_native_command_recorder(vulkan_command_recorder& recorder) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.initialize(device_));
}

granit_result renderer_state::begin_command_recorder(vulkan_command_recorder& recorder) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.begin(device_));
}

granit_result renderer_state::end_command_recorder(vulkan_command_recorder& recorder) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.end(device_));
}

granit_result renderer_state::reset_command_recorder(vulkan_command_recorder& recorder) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.reset(device_));
}

granit_result renderer_state::copy_buffer(vulkan_command_recorder& recorder, VkBuffer source,
                                          VkBuffer destination,
                                          std::span<const VkBufferCopy> regions) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.copy_buffer(device_, source, destination, regions));
}

granit_result renderer_state::fill_buffer(vulkan_command_recorder& recorder, VkBuffer buffer,
                                          VkDeviceSize offset, VkDeviceSize size,
                                          std::uint32_t value) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.fill_buffer(device_, buffer, offset, size, value));
}

granit_result renderer_state::bind_graphics_pipeline(vulkan_command_recorder& recorder,
                                                     VkPipeline pipeline) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return recorder.bind_graphics_pipeline(device_, pipeline);
}

granit_result
renderer_state::bind_graphics_groups(vulkan_command_recorder& recorder, VkPipelineLayout layout,
                                     std::uint32_t first_group,
                                     std::span<const VkDescriptorSet> bind_groups) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return recorder.bind_graphics_groups(device_, layout, first_group, bind_groups);
}

granit_result renderer_state::bind_compute_pipeline(vulkan_command_recorder& recorder,
                                                    VkPipeline pipeline) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.bind_compute_pipeline(device_, pipeline);
}

granit_result renderer_state::bind_compute_groups(
    vulkan_command_recorder& recorder, VkPipelineLayout layout, std::uint32_t first_group,
    std::span<const VkDescriptorSet> bind_groups,
    std::span<const std::pair<VkBuffer, VkAccessFlags2>> buffer_accesses,
    std::span<const vulkan_image_access> image_accesses) {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.bind_compute_groups(device_, layout, first_group, bind_groups,
                                                      buffer_accesses, image_accesses);
}

granit_result renderer_state::dispatch(vulkan_command_recorder& recorder,
                                       std::uint32_t group_count_x, std::uint32_t group_count_y,
                                       std::uint32_t group_count_z) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.dispatch(device_, group_count_x, group_count_y, group_count_z);
}

granit_result renderer_state::set_viewports(vulkan_command_recorder& recorder, std::uint32_t first,
                                            std::span<const VkViewport> viewports) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.set_viewports(device_, first, viewports);
}

granit_result renderer_state::set_scissors(vulkan_command_recorder& recorder, std::uint32_t first,
                                           std::span<const VkRect2D> scissors) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST : recorder.set_scissors(device_, first, scissors);
}

granit_result renderer_state::bind_vertex_buffers(vulkan_command_recorder& recorder,
                                                  std::uint32_t first,
                                                  std::span<const VkBuffer> buffers,
                                                  std::span<const VkDeviceSize> offsets) {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.bind_vertex_buffers(device_, first, buffers, offsets);
}

granit_result renderer_state::bind_index_buffer(vulkan_command_recorder& recorder, VkBuffer buffer,
                                                VkDeviceSize offset, VkIndexType type) {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.bind_index_buffer(device_, buffer, offset, type);
}

granit_result renderer_state::draw(vulkan_command_recorder& recorder, std::uint32_t vertex_count,
                                   std::uint32_t instance_count, std::uint32_t first_vertex,
                                   std::uint32_t first_instance) noexcept {
  return device_lost()
             ? GRANIT_ERROR_DEVICE_LOST
             : recorder.draw(device_, vertex_count, instance_count, first_vertex, first_instance);
}

granit_result renderer_state::draw_indexed(vulkan_command_recorder& recorder,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.draw_indexed(device_, index_count, instance_count, first_index,
                                               vertex_offset, first_instance);
}

granit_result
renderer_state::begin_rendering(vulkan_command_recorder& recorder, VkRect2D area,
                                std::span<const VkRenderingAttachmentInfo> color_attachments,
                                const VkRenderingAttachmentInfo* depth_attachment,
                                const VkRenderingAttachmentInfo* stencil_attachment,
                                std::uint32_t layer_count,
                                std::span<const vulkan_image_access> image_accesses) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.begin_rendering(device_, area, color_attachments,
                                                        depth_attachment, stencil_attachment,
                                                        layer_count, image_accesses));
}

granit_result renderer_state::end_rendering(vulkan_command_recorder& recorder) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  return observe_device_result(recorder.end_rendering(device_));
}

granit_result renderer_state::complete_frame_slot(frame_slot& slot) noexcept {
  if (slot.serial == 0) {
    return GRANIT_SUCCESS;
  }
  const auto result = slot.context->wait(device_, UINT64_MAX);
  if (result != GRANIT_SUCCESS) {
    return observe_device_result(result);
  }
  if (slot.recorder != nullptr)
    slot.recorder->mark_complete();
  submission_serials_.mark_completed(slot.serial);
  slot.recorder = nullptr;
  slot.serial = 0;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::submit_command_recorder(vulkan_command_recorder& recorder,
                                                      submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (recorder.state() != command_recorder_state::executable || frame_slots_.empty()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto& slot = frame_slots_[next_frame_slot_];
  if (slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto result = complete_frame_slot(slot);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const auto serial = submission_serials_.next();
  if (serial == 0) {
    return GRANIT_ERROR_INTERNAL;
  }
  std::vector<VkImageMemoryBarrier2> image_barriers;
  image_barriers.reserve(recorder.initial_image_accesses().size());
  image_states_.reserve(image_states_.size() + recorder.final_image_accesses().size());
  for (const auto& destination : recorder.initial_image_accesses()) {
    const auto previous =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& state) { return state.image == destination.image; });
    if (previous == image_states_.end() && destination.preserve_content) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask =
        previous == image_states_.end() ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : previous->stages;
    barrier.srcAccessMask = previous == image_states_.end() ? 0 : previous->access;
    barrier.dstStageMask = destination.stages;
    barrier.dstAccessMask = destination.access;
    barrier.oldLayout =
        previous == image_states_.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
    barrier.newLayout = destination.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = destination.image;
    barrier.subresourceRange = destination.range;
    image_barriers.push_back(barrier);
  }
  const bool use_preamble = !image_barriers.empty();
  if (use_preamble) {
    if (slot.preamble->state() == command_recorder_state::executable) {
      result = slot.preamble->reset(device_);
      if (result != GRANIT_SUCCESS)
        return observe_device_result(result);
    }
    result = slot.preamble->begin(device_);
    if (result == GRANIT_SUCCESS)
      result = slot.preamble->record_image_barriers(device_, image_barriers);
    if (result == GRANIT_SUCCESS)
      result = slot.preamble->end(device_);
    if (result != GRANIT_SUCCESS)
      return observe_device_result(result);
  }
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS) {
    return observe_device_result(result);
  }
  std::array<VkCommandBufferSubmitInfo, 2> command_infos{};
  std::uint32_t command_count{};
  if (use_preamble) {
    command_infos[command_count].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_infos[command_count++].commandBuffer = slot.preamble->native_handle();
  }
  command_infos[command_count].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command_infos[command_count++].commandBuffer = recorder.native_handle();
  VkSubmitInfo2 submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.commandBufferInfoCount = command_count;
  submit_info.pCommandBufferInfos = command_infos.data();
  const auto submit_result = device_.functions().vkQueueSubmit2(
      device_.graphics_queue(), 1, &submit_info, slot.context->completion_fence());
  if (submit_result != VK_SUCCESS) {
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(submit_result));
  }
  static_cast<void>(recorder.mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  for (const auto& final : recorder.final_image_accesses()) {
    const auto state =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& current) { return current.image == final.image; });
    if (state == image_states_.end())
      image_states_.push_back(final);
    else
      *state = final;
  }
  slot.recorder = &recorder;
  slot.serial = serial;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result renderer_state::acquire_swapchain_frame(vulkan_swapchain& swapchain,
                                                      std::uint32_t& image_index,
                                                      std::size_t& slot_index,
                                                      bool& needs_recreate) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  auto& slot = frame_slots_[next_frame_slot_];
  if (slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto complete_result = complete_frame_slot(slot);
  if (complete_result != GRANIT_SUCCESS)
    return complete_result;
  const auto acquired = swapchain.acquire(device_, slot.context->image_available());
  if (acquired.result != GRANIT_SUCCESS)
    return observe_device_result(acquired.result);
  slot.acquired = true;
  image_index = acquired.image_index;
  slot_index = next_frame_slot_;
  needs_recreate = acquired.suboptimal;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::submit_swapchain_frame(vulkan_command_recorder& recorder,
                                                     vulkan_swapchain& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (slot_index >= frame_slots_.size() || image_index >= swapchain.images().size() ||
      recorder.state() != command_recorder_state::executable)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[slot_index];
  if (!slot.acquired || slot.awaiting_present || slot_index != next_frame_slot_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto image = swapchain.images()[image_index];
  const auto final =
      std::find_if(recorder.final_image_accesses().begin(), recorder.final_image_accesses().end(),
                   [&](const auto& access) { return access.image == image; });
  if (final == recorder.final_image_accesses().end())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<VkImageMemoryBarrier2> initial_barriers;
  initial_barriers.reserve(recorder.initial_image_accesses().size());
  image_states_.reserve(image_states_.size() + recorder.final_image_accesses().size());
  for (const auto& destination : recorder.initial_image_accesses()) {
    const auto previous =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& state) { return state.image == destination.image; });
    if (previous == image_states_.end() && destination.preserve_content)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask =
        previous == image_states_.end() ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : previous->stages;
    barrier.srcAccessMask = previous == image_states_.end() ? 0 : previous->access;
    barrier.dstStageMask = destination.stages;
    barrier.dstAccessMask = destination.access;
    barrier.oldLayout =
        previous == image_states_.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
    barrier.newLayout = destination.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = destination.image;
    barrier.subresourceRange = destination.range;
    initial_barriers.push_back(barrier);
  }
  auto result = slot.preamble->state() == command_recorder_state::executable
                    ? slot.preamble->reset(device_)
                    : GRANIT_SUCCESS;
  if (result == GRANIT_SUCCESS)
    result = slot.preamble->begin(device_);
  if (result == GRANIT_SUCCESS)
    result = slot.preamble->record_image_barriers(device_, initial_barriers);
  if (result == GRANIT_SUCCESS)
    result = slot.preamble->end(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);

  if (slot.postamble->state() == command_recorder_state::executable)
    result = slot.postamble->reset(device_);
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->begin(device_);
  VkImageMemoryBarrier2 present_barrier{};
  present_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  present_barrier.srcStageMask = final->stages;
  present_barrier.srcAccessMask = final->access;
  present_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  present_barrier.dstAccessMask = 0;
  present_barrier.oldLayout = final->layout;
  present_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  present_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  present_barrier.image = image;
  present_barrier.subresourceRange = final->range;
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->record_image_barriers(device_, {&present_barrier, 1});
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->end(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);

  const auto serial = submission_serials_.next();
  if (serial == 0)
    return GRANIT_ERROR_INTERNAL;
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);
  std::array<VkCommandBufferSubmitInfo, 3> commands{};
  const VkCommandBuffer buffers[]{slot.preamble->native_handle(), recorder.native_handle(),
                                  slot.postamble->native_handle()};
  for (std::size_t index = 0; index < commands.size(); ++index) {
    commands[index].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commands[index].commandBuffer = buffers[index];
  }
  VkSemaphoreSubmitInfo wait{};
  wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait.semaphore = slot.context->image_available();
  wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSemaphoreSubmitInfo signal{};
  signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal.semaphore = swapchain.render_finished(image_index);
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.commandBufferInfoCount = static_cast<std::uint32_t>(commands.size());
  submit.pCommandBufferInfos = commands.data();
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  const auto queue_result = device_.functions().vkQueueSubmit2(device_.graphics_queue(), 1, &submit,
                                                               slot.context->completion_fence());
  if (queue_result != VK_SUCCESS) {
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(queue_result));
  }
  static_cast<void>(recorder.mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  for (const auto& access : recorder.final_image_accesses()) {
    const auto state =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& current) { return current.image == access.image; });
    if (state == image_states_.end())
      image_states_.push_back(access);
    else
      *state = access;
  }
  auto present_state = *final;
  present_state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_state.stages = VK_PIPELINE_STAGE_2_NONE;
  present_state.access = 0;
  const auto state = std::find_if(image_states_.begin(), image_states_.end(),
                                  [&](const auto& current) { return current.image == image; });
  *state = present_state;
  slot.recorder = &recorder;
  slot.serial = serial;
  slot.acquired = false;
  slot.awaiting_present = true;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result renderer_state::present_swapchain_frame(vulkan_swapchain& swapchain,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index,
                                                      bool& needs_recreate) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (slot_index >= frame_slots_.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[slot_index];
  if (!slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto presented = swapchain.present(device_, device_.graphics_queue(), image_index,
                                           swapchain.render_finished(image_index));
  slot.awaiting_present = false;
  needs_recreate = presented.suboptimal || presented.result == GRANIT_ERROR_OUT_OF_DATE;
  return observe_device_result(presented.result);
}

granit_result renderer_state::cancel_swapchain_frame(vulkan_swapchain& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index, bool& needs_recreate) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (slot_index >= frame_slots_.size() || image_index >= swapchain.images().size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[slot_index];
  if (!slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto image = swapchain.images()[image_index];
  image_states_.reserve(image_states_.size() + 1);
  const auto previous = std::find_if(image_states_.begin(), image_states_.end(),
                                     [&](const auto& state) { return state.image == image; });
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask =
      previous == image_states_.end() ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : previous->stages;
  barrier.srcAccessMask = previous == image_states_.end() ? 0 : previous->access;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  barrier.oldLayout =
      previous == image_states_.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .baseMipLevel = 0,
                              .levelCount = 1,
                              .baseArrayLayer = 0,
                              .layerCount = 1};
  auto result = slot.postamble->state() == command_recorder_state::executable
                    ? slot.postamble->reset(device_)
                    : GRANIT_SUCCESS;
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->begin(device_);
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->record_image_barriers(device_, {&barrier, 1});
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->end(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);
  const auto serial = submission_serials_.next();
  if (serial == 0)
    return GRANIT_ERROR_INTERNAL;
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS)
    return result;
  VkSemaphoreSubmitInfo wait{};
  wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait.semaphore = slot.context->image_available();
  wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkCommandBufferSubmitInfo command{};
  command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command.commandBuffer = slot.postamble->native_handle();
  VkSemaphoreSubmitInfo signal{};
  signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal.semaphore = swapchain.render_finished(image_index);
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &command;
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  const auto queue_result = device_.functions().vkQueueSubmit2(device_.graphics_queue(), 1, &submit,
                                                               slot.context->completion_fence());
  if (queue_result != VK_SUCCESS) {
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(queue_result));
  }
  static_cast<void>(submission_serials_.commit(serial));
  slot.serial = serial;
  slot.acquired = false;
  slot.awaiting_present = true;
  const auto presented = swapchain.present(device_, device_.graphics_queue(), image_index,
                                           swapchain.render_finished(image_index));
  slot.awaiting_present = false;
  needs_recreate = presented.suboptimal || presented.result == GRANIT_ERROR_OUT_OF_DATE;
  vulkan_image_access present_state{};
  present_state.image = image;
  present_state.range = barrier.subresourceRange;
  present_state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  if (previous == image_states_.end())
    image_states_.push_back(present_state);
  else
    *previous = present_state;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return observe_device_result(presented.result);
}

granit_result renderer_state::observe_device_result(granit_result result) noexcept {
  return device_status_.observe(result);
}

granit_result renderer_state::wait_command_recorder(vulkan_command_recorder& recorder) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (recorder.state() != command_recorder_state::pending) {
    return GRANIT_SUCCESS;
  }
  for (auto& slot : frame_slots_) {
    if (slot.recorder == &recorder) {
      return complete_frame_slot(slot);
    }
  }
  return GRANIT_ERROR_INTERNAL;
}

granit_result renderer_state::wait_for_all_submissions() noexcept {
  std::lock_guard lock{queue_mutex_};
  for (auto& slot : frame_slots_) {
    const auto result = complete_frame_slot(slot);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  return GRANIT_SUCCESS;
}

granit_result renderer_state::wait_for_present_idle() noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  const auto result = observe_device_result(
      map_vulkan_result(device_.functions().vkQueueWaitIdle(device_.graphics_queue())));
  if (result == GRANIT_SUCCESS)
    submission_serials_.mark_completed(submission_serials_.last_submitted());
  return result;
}

void renderer_state::retire_resource(submission_serial retire_after, retirement_order order,
                                     std::shared_ptr<void> resource) {
  std::lock_guard lock{retirement_mutex_};
  retirement_queue_.retire(retire_after, order, std::move(resource));
}

std::size_t renderer_state::collect_retired() noexcept {
  submission_serial completed{};
  {
    std::lock_guard lock{queue_mutex_};
    completed = submission_serials_.completed();
  }
  std::lock_guard lock{retirement_mutex_};
  return retirement_queue_.collect(completed);
}

std::size_t renderer_state::drain_retired() noexcept {
  std::lock_guard lock{retirement_mutex_};
  return retirement_queue_.drain();
}

void renderer_state::destroy_native_command_recorder(vulkan_command_recorder& recorder) noexcept {
  recorder.destroy(device_);
}

} // namespace granit::detail
