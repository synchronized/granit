// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

#include "backend/vulkan/resources.h"
#include "backend/vulkan/result.h"
#include "backend/vulkan/surface.h"
#include "core/texture_format.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace granit::detail {
namespace {

template <typename Handle> std::uint64_t object_handle_value(Handle handle) noexcept {
  if constexpr (std::is_pointer_v<Handle>)
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
  else
    return static_cast<std::uint64_t>(handle);
}

granit_texture_format map_swapchain_format(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_B8G8R8A8_SRGB:
    return GRANIT_TEXTURE_FORMAT_BGRA8_SRGB;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  default:
    return GRANIT_TEXTURE_FORMAT_UNDEFINED;
  }
}

bool same_image_subresource(const vulkan_image_access& left,
                            const vulkan_image_access& right) noexcept {
  return left.image == right.image && left.range.aspectMask == right.range.aspectMask &&
         left.range.baseMipLevel == right.range.baseMipLevel &&
         left.range.levelCount == right.range.levelCount &&
         left.range.baseArrayLayer == right.range.baseArrayLayer &&
         left.range.layerCount == right.range.layerCount;
}

template <typename States>
auto find_image_subresource(States& states, const vulkan_image_access& access) {
  return std::find_if(states.begin(), states.end(),
                      [&](const auto& state) { return same_image_subresource(state, access); });
}

void store_unit_image_accesses(std::vector<vulkan_image_access>& states,
                               const vulkan_image_access& access) {
  constexpr std::array aspects{VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                               VK_IMAGE_ASPECT_STENCIL_BIT};
  for (const auto aspect : aspects) {
    if ((access.range.aspectMask & aspect) == 0)
      continue;
    for (std::uint32_t mip_offset = 0; mip_offset < access.range.levelCount; ++mip_offset) {
      for (std::uint32_t layer_offset = 0; layer_offset < access.range.layerCount; ++layer_offset) {
        auto unit = access;
        unit.range = {static_cast<VkImageAspectFlags>(aspect),
                      access.range.baseMipLevel + mip_offset, 1,
                      access.range.baseArrayLayer + layer_offset, 1};
        const auto found = find_image_subresource(states, unit);
        if (found == states.end())
          states.push_back(unit);
        else
          *found = unit;
      }
    }
  }
}

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

VkAttachmentLoadOp map_attachment_load(granit_attachment_load_operation value) noexcept {
  return value == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
             ? VK_ATTACHMENT_LOAD_OP_LOAD
             : (value == GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                                : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
}

VkAttachmentStoreOp map_attachment_store(granit_attachment_store_operation value) noexcept {
  return value == GRANIT_ATTACHMENT_STORE_OPERATION_STORE ? VK_ATTACHMENT_STORE_OP_STORE
                                                          : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkComponentSwizzle map_component_swizzle(granit_component_swizzle swizzle) noexcept {
  constexpr std::array values{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_ZERO,
                              VK_COMPONENT_SWIZZLE_ONE,      VK_COMPONENT_SWIZZLE_R,
                              VK_COMPONENT_SWIZZLE_G,        VK_COMPONENT_SWIZZLE_B,
                              VK_COMPONENT_SWIZZLE_A};
  return values[swizzle];
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
  for (auto& slot : upload_slots_)
    slot.context->destroy(device_, memory_allocator_);
  upload_slots_.clear();
  for (auto& slot : frame_slots_) {
    for (auto& preamble : slot.batch_preambles)
      preamble->destroy(device_);
    slot.postamble->destroy(device_);
    slot.preamble->destroy(device_);
    slot.context->destroy(device_);
  }
  frame_slots_.clear();
  if (pipeline_cache_ != VK_NULL_HANDLE)
    device_.functions().vkDestroyPipelineCache(device_.native_handle(), pipeline_cache_, nullptr);
}

granit_result renderer_state::initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types,
                                         std::uint32_t frames_in_flight,
                                         granit_diagnostic_callback diagnostic_callback,
                                         void* diagnostic_user_data) {
  validation_enabled_ = enable_validation;
  diagnostics_.configure(diagnostic_callback, diagnostic_user_data);
  const auto instance_result = instance_.initialize({.application_name = application_name,
                                                     .enable_validation = enable_validation,
                                                     .surface_types = surface_types,
                                                     .diagnostics = &diagnostics_});
  if (instance_result != GRANIT_SUCCESS) {
    return instance_result;
  }

  const auto device_result = device_.initialize(instance_, surface_types);
  if (device_result != GRANIT_SUCCESS) {
    instance_.reset();
    return device_result;
  }
  const auto& limits = device_.properties().limits;
  capabilities_ = {
      .uniform_buffer_offset_alignment = limits.minUniformBufferOffsetAlignment,
      .storage_buffer_offset_alignment = limits.minStorageBufferOffsetAlignment,
      .max_uniform_buffer_binding_size = limits.maxUniformBufferRange,
      .max_storage_buffer_binding_size = limits.maxStorageBufferRange,
  };

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
                      .postamble = std::make_unique<vulkan_command_recorder>(),
                      .batch_preambles = {},
                      .recorders = {},
                      .serial = 0,
                      .acquired = false,
                      .awaiting_present = false};
      slot.recorders.reserve(1);
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
  try {
    upload_slots_.reserve(frames_in_flight);
    for (std::uint32_t index = 0; index < frames_in_flight; ++index) {
      upload_slot slot{.context = std::make_unique<vulkan_upload_context>(), .acquired = false};
      const auto upload_result = slot.context->initialize(device_);
      if (upload_result != GRANIT_SUCCESS) {
        for (auto& initialized : upload_slots_)
          initialized.context->destroy(device_, memory_allocator_);
        upload_slots_.clear();
        for (auto& initialized : frame_slots_) {
          initialized.postamble->destroy(device_);
          initialized.preamble->destroy(device_);
          initialized.context->destroy(device_);
        }
        frame_slots_.clear();
        memory_allocator_.reset();
        device_.reset();
        instance_.reset();
        return upload_result;
      }
      upload_slots_.push_back(std::move(slot));
    }
  } catch (...) {
    for (auto& slot : upload_slots_)
      slot.context->destroy(device_, memory_allocator_);
    upload_slots_.clear();
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
  VkPipelineCacheCreateInfo cache_info{};
  cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  const auto cache_result = device_.functions().vkCreatePipelineCache(
      device_.native_handle(), &cache_info, nullptr, &pipeline_cache_);
  if (cache_result != VK_SUCCESS) {
    for (auto& slot : upload_slots_)
      slot.context->destroy(device_, memory_allocator_);
    upload_slots_.clear();
    for (auto& slot : frame_slots_) {
      slot.postamble->destroy(device_);
      slot.preamble->destroy(device_);
      slot.context->destroy(device_);
    }
    frame_slots_.clear();
    memory_allocator_.reset();
    device_.reset();
    instance_.reset();
    return map_vulkan_result(cache_result);
  }
  surface_types_ = surface_types;
  lifecycle_.mark_ready();
  return GRANIT_SUCCESS;
}

std::size_t renderer_state::acquire_upload_slot() {
  std::unique_lock lock{upload_mutex_};
  upload_available_.wait(lock, [this] {
    return std::ranges::any_of(upload_slots_, [](const auto& slot) { return !slot.acquired; });
  });
  const auto found =
      std::ranges::find_if(upload_slots_, [](const auto& slot) { return !slot.acquired; });
  found->acquired = true;
  return static_cast<std::size_t>(std::distance(upload_slots_.begin(), found));
}

void renderer_state::release_upload_slot(std::size_t index) noexcept {
  {
    std::lock_guard lock{upload_mutex_};
    upload_slots_[index].acquired = false;
  }
  upload_available_.notify_one();
}

granit_result renderer_state::import_pipeline_cache(const void* data, std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  if (size == 0)
    return GRANIT_SUCCESS;
  if (!data || size > SIZE_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  struct cache_header {
    std::uint32_t length;
    std::uint32_t version;
    std::uint32_t vendor;
    std::uint32_t device;
    std::array<std::uint8_t, VK_UUID_SIZE> uuid;
  };
  if (size < sizeof(cache_header))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  cache_header header{};
  std::memcpy(&header, data, sizeof(header));
  const auto& properties = device_.properties();
  if (header.length < sizeof(cache_header) || header.length > size ||
      header.version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE ||
      header.vendor != properties.vendorID || header.device != properties.deviceID ||
      std::memcmp(header.uuid.data(), properties.pipelineCacheUUID, VK_UUID_SIZE) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard lock{pipeline_cache_mutex_};
  VkPipelineCacheCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  info.initialDataSize = static_cast<std::size_t>(size);
  info.pInitialData = data;
  VkPipelineCache imported = VK_NULL_HANDLE;
  const auto created =
      device_.functions().vkCreatePipelineCache(device_.native_handle(), &info, nullptr, &imported);
  if (created != VK_SUCCESS)
    return observe_device_result(map_vulkan_result(created));
  const auto merged = device_.functions().vkMergePipelineCaches(device_.native_handle(),
                                                                pipeline_cache_, 1, &imported);
  device_.functions().vkDestroyPipelineCache(device_.native_handle(), imported, nullptr);
  return observe_device_result(map_vulkan_result(merged));
}

granit_result renderer_state::export_pipeline_cache(void* data, std::uint64_t& size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{pipeline_cache_mutex_};
  std::size_t required{};
  auto result = device_.functions().vkGetPipelineCacheData(device_.native_handle(), pipeline_cache_,
                                                           &required, nullptr);
  if (result != VK_SUCCESS)
    return observe_device_result(map_vulkan_result(result));
  if (!data) {
    size = required;
    return GRANIT_SUCCESS;
  }
  if (size < required) {
    size = required;
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto capacity = static_cast<std::size_t>(size);
  result = device_.functions().vkGetPipelineCacheData(device_.native_handle(), pipeline_cache_,
                                                      &capacity, data);
  size = capacity;
  return observe_device_result(map_vulkan_result(result));
}

granit_result renderer_state::set_object_name(VkObjectType type, std::uint64_t object,
                                              std::string_view name) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  if (!validation_enabled_ || instance_.functions().vkSetDebugUtilsObjectNameEXT == nullptr)
    return GRANIT_ERROR_UNSUPPORTED;

  const std::string terminated{name};
  VkDebugUtilsObjectNameInfoEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  info.objectType = type;
  info.objectHandle = object;
  info.pObjectName = terminated.c_str();
  return observe_device_result(map_vulkan_result(
      instance_.functions().vkSetDebugUtilsObjectNameEXT(device_.native_handle(), &info)));
}

granit_result renderer_state::set_backend_resource_name(backend_resource& resource,
                                                        std::string_view name) {
  if (auto* value = dynamic_cast<vulkan_surface_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_SURFACE_KHR, object_handle_value(value->native()), name);
  if (auto* value = dynamic_cast<vulkan_swapchain_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_SWAPCHAIN_KHR,
                           object_handle_value(value->native().native_handle()), name);
  if (auto* value = dynamic_cast<vulkan_buffer_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_BUFFER, object_handle_value(value->native().buffer),
                           name);
  if (auto* value = dynamic_cast<vulkan_texture_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_IMAGE, object_handle_value(value->native().image), name);
  if (auto* value = dynamic_cast<vulkan_texture_view_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, object_handle_value(value->native()), name);
  if (auto* value = dynamic_cast<vulkan_sampler_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_SAMPLER, object_handle_value(value->native()), name);
  if (auto* value = dynamic_cast<vulkan_shader_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_SHADER_MODULE, object_handle_value(value->native()),
                           name);
  if (auto* value = dynamic_cast<vulkan_bind_group_layout_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                           object_handle_value(value->native()), name);
  if (auto* value = dynamic_cast<vulkan_pipeline_layout_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, object_handle_value(value->native()),
                           name);
  if (auto* value = dynamic_cast<vulkan_bind_group_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET, object_handle_value(value->set()), name);
  if (auto* value = dynamic_cast<vulkan_graphics_pipeline_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_PIPELINE, object_handle_value(value->native()), name);
  if (auto* value = dynamic_cast<vulkan_compute_pipeline_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_PIPELINE, object_handle_value(value->native()), name);
  if (auto* value = dynamic_cast<vulkan_command_recorder_resource*>(&resource))
    return set_object_name(VK_OBJECT_TYPE_COMMAND_BUFFER,
                           object_handle_value(value->native().native_handle()), name);
  if (auto* value = dynamic_cast<vulkan_timestamp_query_pool_resource*>(&resource))
    return set_timestamp_query_pool_name(*value, name);
  return GRANIT_ERROR_UNSUPPORTED;
}

std::unique_ptr<backend_surface_resource> renderer_state::allocate_surface_resource() {
  return std::make_unique<vulkan_surface_resource>(shared_from_this());
}

std::unique_ptr<backend_swapchain_resource> renderer_state::allocate_swapchain_resource() {
  return std::make_unique<vulkan_swapchain_resource>(shared_from_this());
}

std::unique_ptr<backend_buffer_resource> renderer_state::allocate_buffer_resource() {
  return std::make_unique<vulkan_buffer_resource>(shared_from_this());
}

std::unique_ptr<backend_texture_resource> renderer_state::allocate_texture_resource() {
  return std::make_unique<vulkan_texture_resource>(shared_from_this());
}

std::unique_ptr<backend_texture_view_resource> renderer_state::allocate_texture_view_resource() {
  return std::make_unique<vulkan_texture_view_resource>(shared_from_this());
}

std::unique_ptr<backend_sampler_resource> renderer_state::allocate_sampler_resource() {
  return std::make_unique<vulkan_sampler_resource>(shared_from_this());
}

std::unique_ptr<backend_shader_resource> renderer_state::allocate_shader_resource() {
  return std::make_unique<vulkan_shader_resource>(shared_from_this());
}

std::unique_ptr<backend_bind_group_layout_resource>
renderer_state::allocate_bind_group_layout_resource() {
  return std::make_unique<vulkan_bind_group_layout_resource>(shared_from_this());
}

std::unique_ptr<backend_bind_group_resource> renderer_state::allocate_bind_group_resource() {
  return std::make_unique<vulkan_bind_group_resource>(shared_from_this());
}

std::unique_ptr<backend_pipeline_layout_resource>
renderer_state::allocate_pipeline_layout_resource() {
  return std::make_unique<vulkan_pipeline_layout_resource>(shared_from_this());
}

std::unique_ptr<backend_graphics_pipeline_resource>
renderer_state::allocate_graphics_pipeline_resource() {
  return std::make_unique<vulkan_graphics_pipeline_resource>(shared_from_this());
}

std::unique_ptr<backend_compute_pipeline_resource>
renderer_state::allocate_compute_pipeline_resource() {
  return std::make_unique<vulkan_compute_pipeline_resource>(shared_from_this());
}

std::unique_ptr<backend_command_recorder_resource>
renderer_state::allocate_command_recorder_resource() {
  return std::make_unique<vulkan_command_recorder_resource>(shared_from_this());
}

granit_result
renderer_state::create_win32_surface(void* native_instance, void* native_window,
                                     backend_surface_resource& surface_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  auto& surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  return observe_device_result(
      detail::create_win32_surface(instance_, device_, native_instance, native_window, surface));
}

granit_result
renderer_state::create_xcb_surface(void* connection, std::uint32_t window,
                                   backend_surface_resource& surface_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_XCB_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  auto& surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  return observe_device_result(
      detail::create_xcb_surface(instance_, device_, connection, window, surface));
}

granit_result
renderer_state::create_wayland_surface(void* display, void* native_surface,
                                       backend_surface_resource& surface_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WAYLAND_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  auto& surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  return observe_device_result(
      detail::create_wayland_surface(instance_, device_, display, native_surface, surface));
}

granit_result renderer_state::create_canvas_surface(std::string_view,
                                                    backend_surface_resource&) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST : GRANIT_ERROR_UNSUPPORTED;
}

void renderer_state::destroy_native_surface(VkSurfaceKHR surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  detail::destroy_surface(instance_, surface);
}

granit_result renderer_state::create_swapchain(backend_surface_resource& surface_resource,
                                               const backend_swapchain_desc& desc,
                                               backend_swapchain_resource& swapchain_resource) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native();
  const vulkan_swapchain_desc native_desc{desc.width, desc.height, desc.minimum_image_count,
                                          desc.present_mode};
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(swapchain.initialize(instance_, device_, surface, native_desc));
}

granit_result renderer_state::recreate_swapchain(backend_surface_resource& surface_resource,
                                                 const backend_swapchain_desc& desc,
                                                 backend_swapchain_resource& swapchain_resource) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native();
  const vulkan_swapchain_desc native_desc{desc.width, desc.height, desc.minimum_image_count,
                                          desc.present_mode};
  std::lock_guard lock{resource_mutex_};
  {
    std::lock_guard queue_lock{queue_mutex_};
    for (const auto image : swapchain.images()) {
      std::erase_if(image_states_, [&](const auto& state) { return state.image == image; });
    }
  }
  return observe_device_result(swapchain.recreate(instance_, device_, surface, native_desc));
}

backend_swapchain_info
renderer_state::get_swapchain_info(backend_swapchain_resource& swapchain_resource) noexcept {
  std::lock_guard lock{resource_mutex_};
  const auto info = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native().info();
  return {info.width, info.height, info.image_count, info.present_mode,
          map_swapchain_format(info.format)};
}

granit_result
renderer_state::get_swapchain_backbuffers(backend_swapchain_resource& swapchain_resource,
                                          std::vector<backend_swapchain_backbuffer>& backbuffers) {
  try {
    std::lock_guard lock{resource_mutex_};
    auto& swapchain = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native();
    const auto info = swapchain.info();
    backbuffers.clear();
    backbuffers.reserve(swapchain.images().size());
    for (const auto image : swapchain.images()) {
      auto texture = std::make_unique<vulkan_texture_resource>(shared_from_this(), false);
      texture->native().image = image;
      granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
      desc.format = map_swapchain_format(info.format);
      desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
      desc.width = info.width;
      desc.height = info.height;
      backbuffers.push_back({std::move(texture), desc});
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
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

granit_result
renderer_state::create_native_buffer(const granit_buffer_desc& desc,
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

void renderer_state::destroy_native_buffer(vulkan_buffer_allocation& buffer) noexcept {
  memory_allocator_.destroy_buffer(buffer);
}

void* renderer_state::mapped_buffer_data(backend_buffer_resource& buffer) noexcept {
  return static_cast<vulkan_buffer_resource&>(buffer).native().mapped_data;
}

granit_result renderer_state::flush_buffer(backend_buffer_resource& buffer_resource,
                                           std::uint64_t offset, std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  return observe_device_result(memory_allocator_.flush(buffer, offset, size));
}

granit_result renderer_state::invalidate_buffer(backend_buffer_resource& buffer_resource,
                                                std::uint64_t offset, std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  return observe_device_result(memory_allocator_.invalidate(buffer, offset, size));
}

granit_result renderer_state::upload_buffer(backend_buffer_resource& buffer_resource,
                                            std::uint64_t offset, const void* data,
                                            std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  const auto slot_index = acquire_upload_slot();
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

  const auto& functions = device_.functions();
  const VkBufferCopy copy{.srcOffset = 0, .dstOffset = offset, .size = size};
  functions.vkCmdCopyBuffer(context.command_buffer(), context.staging().buffer, buffer.buffer, 1,
                            &copy);
  result = context.end(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.reset_fence(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  {
    std::lock_guard queue_lock{queue_mutex_};
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
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(context.restore_signaled_fence(device_));
    return finish(result);
  }
  return finish(context.wait(device_));
}

granit_result
renderer_state::upload_batch(std::span<const backend_upload_operation> uploads) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  if (uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  VkDeviceSize required{};
  const auto alignment =
      std::max<VkDeviceSize>(4, device_.properties().limits.optimalBufferCopyOffsetAlignment);
  for (const auto& upload : uploads) {
    if (upload.data == nullptr || upload.size == 0 ||
        (upload.type != backend_upload_type::buffer &&
         upload.type != backend_upload_type::texture) ||
        (upload.type == backend_upload_type::buffer && upload.buffer == nullptr) ||
        (upload.type == backend_upload_type::texture && upload.texture == nullptr)) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto aligned = (required + alignment - 1) & ~(alignment - 1);
    if (aligned < required || upload.size > UINT64_MAX - aligned)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    required = aligned + upload.size;
  }

  const auto slot_index = acquire_upload_slot();
  auto& context = *upload_slots_[slot_index].context;
  auto finish = [&](granit_result result) {
    release_upload_slot(slot_index);
    return observe_device_result(result);
  };
  auto result = context.ensure_capacity(memory_allocator_, required);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  VkDeviceSize source_offset{};
  for (const auto& upload : uploads) {
    source_offset = (source_offset + alignment - 1) & ~(alignment - 1);
    std::memcpy(static_cast<std::byte*>(context.staging().mapped_data) + source_offset, upload.data,
                static_cast<std::size_t>(upload.size));
    source_offset += upload.size;
  }
  result = memory_allocator_.flush(context.staging(), 0, required);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.begin(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  const auto& functions = device_.functions();
  std::vector<vulkan_image_access> pending_states;
  try {
    pending_states.reserve(uploads.size());
  } catch (const std::bad_alloc&) {
    return finish(GRANIT_ERROR_OUT_OF_MEMORY);
  }
  {
    std::lock_guard queue_lock{queue_mutex_};
    try {
      image_states_.reserve(image_states_.size() + uploads.size());
    } catch (const std::bad_alloc&) {
      return finish(GRANIT_ERROR_OUT_OF_MEMORY);
    }
    source_offset = 0;
    for (const auto& upload : uploads) {
      source_offset = (source_offset + alignment - 1) & ~(alignment - 1);
      if (upload.type == backend_upload_type::buffer) {
        const auto& buffer = static_cast<const vulkan_buffer_resource&>(*upload.buffer).native();
        const VkBufferCopy copy{.srcOffset = source_offset,
                                .dstOffset = upload.destination_offset,
                                .size = upload.size};
        functions.vkCmdCopyBuffer(context.command_buffer(), context.staging().buffer, buffer.buffer,
                                  1, &copy);
      } else {
        const auto& texture = static_cast<const vulkan_texture_resource&>(*upload.texture).native();
        VkBufferImageCopy texture_copy{};
        texture_copy.bufferRowLength = upload.texture_copy.buffer_row_length;
        texture_copy.bufferImageHeight = upload.texture_copy.buffer_image_height;
        texture_copy.imageSubresource = {
            map_texture_aspect(upload.texture_copy.aspect), upload.texture_copy.mip_level,
            upload.texture_copy.base_array_layer, upload.texture_copy.array_layer_count};
        texture_copy.imageOffset = {upload.texture_copy.x, upload.texture_copy.y,
                                    upload.texture_copy.z};
        texture_copy.imageExtent = {upload.texture_copy.width, upload.texture_copy.height,
                                    upload.texture_copy.depth};
        const vulkan_image_access destination{
            .image = texture.image,
            .range = {texture_copy.imageSubresource.aspectMask,
                      texture_copy.imageSubresource.mipLevel, 1,
                      texture_copy.imageSubresource.baseArrayLayer, 1}};
        const auto pending = find_image_subresource(pending_states, destination);
        const auto previous = pending != pending_states.end()
                                  ? pending
                                  : find_image_subresource(image_states_, destination);
        const bool previous_is_pending = pending != pending_states.end();
        const bool has_previous = previous_is_pending || previous != image_states_.end();
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask =
            has_previous ? previous->stages : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = has_previous ? previous->access : 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.oldLayout = has_previous ? previous->layout : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange = {
            texture_copy.imageSubresource.aspectMask, texture_copy.imageSubresource.mipLevel, 1,
            texture_copy.imageSubresource.baseArrayLayer, texture_copy.imageSubresource.layerCount};
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        functions.vkCmdPipelineBarrier2(context.command_buffer(), &dependency);
        auto copy = texture_copy;
        copy.bufferOffset = source_offset;
        functions.vkCmdCopyBufferToImage(context.command_buffer(), context.staging().buffer,
                                         texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                         &copy);
        vulkan_image_access state{.image = texture.image,
                                  .range = {texture_copy.imageSubresource.aspectMask,
                                            texture_copy.imageSubresource.mipLevel, 1,
                                            texture_copy.imageSubresource.baseArrayLayer,
                                            texture_copy.imageSubresource.layerCount},
                                  .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                  .preserve_content = true};
        store_unit_image_accesses(pending_states, state);
      }
      source_offset += upload.size;
    }
    result = context.end(device_);
    if (result == GRANIT_SUCCESS)
      result = context.reset_fence(device_);
    if (result != GRANIT_SUCCESS)
      return finish(result);
    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = context.command_buffer();
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    result = map_vulkan_result(
        functions.vkQueueSubmit2(device_.graphics_queue(), 1, &submit_info, context.fence()));
    if (result == GRANIT_SUCCESS) {
      for (const auto& state : pending_states)
        store_unit_image_accesses(image_states_, state);
    }
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(context.restore_signaled_fence(device_));
    return finish(result);
  }
  return finish(context.wait(device_));
}

granit_result
renderer_state::create_native_texture(const granit_texture_desc& desc,
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
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = map_texture_usage(desc.usage);
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  return observe_device_result(
      memory_allocator_.create_image(info, map_memory_location(desc.memory_location), texture));
}

bool renderer_state::texture_supports_linear_blit(granit_texture_format format) const noexcept {
  return physical_device_supports_linear_blit(instance_, device_, map_texture_format(format));
}

granit_result renderer_state::upload_texture(backend_texture_resource& resource,
                                             granit_texture_format format, const void* data,
                                             std::uint64_t size,
                                             const granit_texture_data_layout& layout,
                                             const granit_texture_write_region& region) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& texture = static_cast<vulkan_texture_resource&>(resource).native();
  const auto bytes_per_pixel = texture_format_bytes_per_block(format);
  if (bytes_per_pixel == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  VkBufferImageCopy copy{};
  copy.bufferRowLength = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel;
  copy.bufferImageHeight = layout.rows_per_image;
  copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                           region.base_array_layer, region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  const auto slot_index = acquire_upload_slot();
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

void renderer_state::destroy_native_texture(vulkan_image_allocation& texture) noexcept {
  {
    std::lock_guard lock{queue_mutex_};
    std::erase_if(image_states_, [&](const auto& state) { return state.image == texture.image; });
  }
  memory_allocator_.destroy_image(texture);
}

granit_result
renderer_state::create_native_texture_view(backend_texture_resource& texture_resource,
                                           const granit_texture_desc& texture_desc,
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

void renderer_state::destroy_native_texture_view(VkImageView view) noexcept {
  if (view != VK_NULL_HANDLE) {
    device_.functions().vkDestroyImageView(device_.native_handle(), view, nullptr);
  }
}

granit_result
renderer_state::create_native_sampler(const granit_sampler_desc& desc,
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

void renderer_state::destroy_native_sampler(VkSampler sampler) noexcept {
  if (sampler != VK_NULL_HANDLE) {
    device_.functions().vkDestroySampler(device_.native_handle(), sampler, nullptr);
  }
}

granit_result
renderer_state::create_native_shader(std::span<const std::uint32_t> code,
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

void renderer_state::destroy_native_shader(VkShaderModule shader) noexcept {
  if (shader != VK_NULL_HANDLE)
    device_.functions().vkDestroyShaderModule(device_.native_handle(), shader, nullptr);
}

granit_result renderer_state::create_native_bind_group_layout(
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

granit_result renderer_state::create_native_bind_group(
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
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case backend_binding_type::storage_texture:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case backend_binding_type::sampler:
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
    else if (write.type == backend_binding_type::sampled_texture)
      index = 3;
    else if (write.type == backend_binding_type::storage_texture)
      index = 4;
    else if (write.type == backend_binding_type::sampler)
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

void renderer_state::destroy_native_bind_group(VkDescriptorPool pool) noexcept {
  if (pool != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyDescriptorPool(device_.native_handle(), pool, nullptr);
  }
}

granit_result renderer_state::create_native_pipeline_layout(
    std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
    backend_pipeline_layout_resource& layout_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  try {
    std::vector<VkDescriptorSetLayout> native_layouts;
    native_layouts.reserve(bind_group_layouts.size());
    for (auto* bind_group_layout : bind_group_layouts) {
      if (bind_group_layout == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_layouts.push_back(
          static_cast<vulkan_bind_group_layout_resource&>(*bind_group_layout).native());
    }
    auto& layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = static_cast<std::uint32_t>(native_layouts.size());
    info.pSetLayouts = native_layouts.data();
    std::lock_guard lock{resource_mutex_};
    return observe_device_result(map_vulkan_result(device_.functions().vkCreatePipelineLayout(
        device_.native_handle(), &info, nullptr, &layout)));
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void renderer_state::destroy_native_pipeline_layout(VkPipelineLayout layout) noexcept {
  if (layout != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipelineLayout(device_.native_handle(), layout, nullptr);
  }
}

granit_result renderer_state::create_native_graphics_pipeline(
    backend_pipeline_layout_resource& layout_resource, backend_shader_resource& vertex_resource,
    const char* vertex_entry, backend_shader_resource& fragment_resource,
    const char* fragment_entry, std::span<const granit_vertex_buffer_layout> vertex_buffers,
    granit_primitive_state primitive, granit_depth_state depth_state,
    const granit_depth_bias_state* depth_bias,
    std::span<const granit_color_blend_state> color_blends,
    std::span<const granit_texture_format> color_formats,
    granit_texture_format depth_stencil_format, granit_sample_count sample_count,
    backend_graphics_pipeline_resource& pipeline_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
  const auto vertex_shader = static_cast<vulkan_shader_resource&>(vertex_resource).native();
  const auto fragment_shader = static_cast<vulkan_shader_resource&>(fragment_resource).native();
  auto& pipeline = static_cast<vulkan_graphics_pipeline_resource&>(pipeline_resource).native();
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
  if (depth_bias) {
    rasterization.depthBiasEnable = VK_TRUE;
    rasterization.depthBiasConstantFactor = depth_bias->constant_factor;
    rasterization.depthBiasSlopeFactor = depth_bias->slope_factor;
    rasterization.depthBiasClamp = depth_bias->clamp;
  }
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
  std::lock_guard lock{pipeline_cache_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateGraphicsPipelines(
      device_.native_handle(), pipeline_cache_, 1, &info, nullptr, &pipeline)));
}

void renderer_state::destroy_native_graphics_pipeline(VkPipeline pipeline) noexcept {
  if (pipeline != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipeline(device_.native_handle(), pipeline, nullptr);
  }
}

granit_result renderer_state::create_native_compute_pipeline(
    backend_pipeline_layout_resource& layout_resource, backend_shader_resource& compute_resource,
    const char* compute_entry, backend_compute_pipeline_resource& pipeline_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
  const auto compute_shader = static_cast<vulkan_shader_resource&>(compute_resource).native();
  auto& pipeline = static_cast<vulkan_compute_pipeline_resource&>(pipeline_resource).native();
  VkComputePipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  info.stage.module = compute_shader;
  info.stage.pName = compute_entry;
  info.layout = layout;
  std::lock_guard lock{pipeline_cache_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateComputePipelines(
      device_.native_handle(), pipeline_cache_, 1, &info, nullptr, &pipeline)));
}

void renderer_state::destroy_native_compute_pipeline(VkPipeline pipeline) noexcept {
  if (pipeline != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipeline(device_.native_handle(), pipeline, nullptr);
  }
}

granit_result renderer_state::create_native_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.initialize(device_));
}

granit_result
renderer_state::begin_command_recorder(backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.begin(device_));
}

granit_result
renderer_state::end_command_recorder(backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.end(device_));
}

granit_result
renderer_state::reset_command_recorder(backend_command_recorder_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  return observe_device_result(recorder.reset(device_));
}

bool renderer_state::command_recorder_is_recording(
    backend_command_recorder_resource& resource) noexcept {
  return static_cast<vulkan_command_recorder_resource&>(resource).native().state() ==
         command_recorder_state::recording;
}

granit_result renderer_state::copy_buffer(backend_command_recorder_resource& recorder_resource,
                                          backend_buffer_resource& source,
                                          backend_buffer_resource& destination,
                                          std::span<const granit_buffer_copy_region> regions) {
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

granit_result renderer_state::copy_texture_to_buffer(
    backend_command_recorder_resource& recorder_resource, backend_texture_resource& source,
    backend_buffer_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const auto bytes_per_pixel = texture_format_bytes_per_block(format);
  VkBufferImageCopy copy{};
  copy.bufferOffset = layout.offset;
  copy.bufferRowLength = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel;
  copy.bufferImageHeight = layout.rows_per_image;
  copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                           region.base_array_layer, region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  return observe_device_result(recorder.copy_texture_to_buffer(
      device_, static_cast<vulkan_texture_resource&>(source).native().image,
      static_cast<vulkan_buffer_resource&>(destination).native().buffer, copy));
}

granit_result renderer_state::copy_buffer_to_texture(
    backend_command_recorder_resource& recorder_resource, backend_buffer_resource& source,
    backend_texture_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  const auto bytes_per_pixel = texture_format_bytes_per_block(format);
  VkBufferImageCopy copy{};
  copy.bufferOffset = layout.offset;
  copy.bufferRowLength = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel;
  copy.bufferImageHeight = layout.rows_per_image;
  copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                           region.base_array_layer, region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  return observe_device_result(recorder.copy_buffer_to_texture(
      device_, static_cast<vulkan_buffer_resource&>(source).native().buffer,
      static_cast<vulkan_texture_resource&>(destination).native().image, copy));
}

granit_result renderer_state::copy_texture(backend_command_recorder_resource& recorder_resource,
                                           backend_texture_resource& source,
                                           backend_texture_resource& destination,
                                           const granit_texture_copy_region& region) {
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

granit_result renderer_state::generate_mipmaps(backend_command_recorder_resource& recorder_resource,
                                               backend_texture_resource& texture,
                                               const granit_texture_desc& desc,
                                               const granit_texture_mipmap_range& range) {
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

granit_result renderer_state::fill_buffer(backend_command_recorder_resource& recorder_resource,
                                          backend_buffer_resource& buffer, std::uint64_t offset,
                                          std::uint64_t size, std::uint32_t value) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return observe_device_result(recorder.fill_buffer(
      device_, static_cast<vulkan_buffer_resource&>(buffer).native().buffer, offset, size, value));
}

granit_result
renderer_state::bind_graphics_pipeline(backend_command_recorder_resource& recorder_resource,
                                       backend_graphics_pipeline_resource& resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return recorder.bind_graphics_pipeline(
      device_, static_cast<vulkan_graphics_pipeline_resource&>(resource).native());
}

granit_result
renderer_state::bind_graphics_groups(backend_command_recorder_resource& recorder_resource,
                                     backend_pipeline_layout_resource& layout_resource,
                                     std::uint32_t first_group,
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
renderer_state::bind_compute_pipeline(backend_command_recorder_resource& recorder_resource,
                                      backend_compute_pipeline_resource& resource) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost()
             ? GRANIT_ERROR_DEVICE_LOST
             : recorder.bind_compute_pipeline(
                   device_, static_cast<vulkan_compute_pipeline_resource&>(resource).native());
}

granit_result
renderer_state::bind_compute_groups(backend_command_recorder_resource& recorder_resource,
                                    backend_pipeline_layout_resource& layout_resource,
                                    std::uint32_t first_group,
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

granit_result renderer_state::dispatch(backend_command_recorder_resource& recorder_resource,
                                       std::uint32_t group_count_x, std::uint32_t group_count_y,
                                       std::uint32_t group_count_z) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.dispatch(device_, group_count_x, group_count_y, group_count_z);
}

granit_result renderer_state::set_viewports(backend_command_recorder_resource& recorder_resource,
                                            std::uint32_t first,
                                            std::span<const granit_viewport> viewports) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  try {
    std::vector<VkViewport> native;
    native.reserve(viewports.size());
    for (const auto& value : viewports)
      native.push_back(
          {value.x, value.y, value.width, value.height, value.min_depth, value.max_depth});
    return recorder.set_viewports(device_, first, native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_state::set_scissors(backend_command_recorder_resource& recorder_resource,
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

granit_result renderer_state::bind_vertex_buffers(
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
renderer_state::bind_index_buffer(backend_command_recorder_resource& recorder_resource,
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

granit_result renderer_state::draw(backend_command_recorder_resource& recorder_resource,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex,
                                   std::uint32_t first_instance) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost()
             ? GRANIT_ERROR_DEVICE_LOST
             : recorder.draw(device_, vertex_count, instance_count, first_vertex, first_instance);
}

granit_result renderer_state::draw_indexed(backend_command_recorder_resource& recorder_resource,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST
                       : recorder.draw_indexed(device_, index_count, instance_count, first_index,
                                               vertex_offset, first_instance);
}

granit_result renderer_state::begin_rendering(
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

granit_result
renderer_state::end_rendering(backend_command_recorder_resource& recorder_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
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
  for (auto* recorder : slot.recorders)
    recorder->mark_complete();
  submission_serials_.mark_completed(slot.serial);
  slot.recorders.clear();
  slot.serial = 0;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::submit_command_recorder(backend_command_recorder_resource& resource,
                                                      submission_serial& submitted_serial) {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (recorder.state() != command_recorder_state::executable || frame_slots_.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[next_frame_slot_];
  if (slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto result = complete_frame_slot(slot);
  if (result != GRANIT_SUCCESS)
    return result;
  const auto serial = submission_serials_.next();
  if (serial == 0)
    return GRANIT_ERROR_INTERNAL;
  std::vector<VkImageMemoryBarrier2> image_barriers;
  image_barriers.reserve(recorder.initial_image_accesses().size());
  image_states_.reserve(image_states_.size() + recorder.final_image_accesses().size());
  for (const auto& destination : recorder.initial_image_accesses()) {
    const auto previous =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& state) { return same_image_subresource(state, destination); });
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
  slot.recorders.clear();
  slot.recorders.push_back(&recorder);
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS) {
    slot.recorders.clear();
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
    slot.recorders.clear();
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(submit_result));
  }
  static_cast<void>(recorder.mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  for (const auto& final : recorder.final_image_accesses()) {
    const auto state =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& current) { return same_image_subresource(current, final); });
    if (state == image_states_.end())
      image_states_.push_back(final);
    else
      *state = final;
  }
  slot.serial = serial;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result renderer_state::submit_command_recorders(
    std::span<backend_command_recorder_resource* const> resources,
    submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::vector<vulkan_command_recorder*> recorders;
  try {
    recorders.reserve(resources.size());
    for (auto* resource : resources) {
      if (resource == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      recorders.push_back(&static_cast<vulkan_command_recorder_resource&>(*resource).native());
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  std::lock_guard lock{queue_mutex_};
  if (recorders.empty() || frame_slots_.empty()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (const auto* recorder : recorders) {
    if (recorder == nullptr || recorder->state() != command_recorder_state::executable)
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
  while (slot.batch_preambles.size() < recorders.size()) {
    auto preamble = std::make_unique<vulkan_command_recorder>();
    result = preamble->initialize(device_);
    if (result != GRANIT_SUCCESS)
      return observe_device_result(result);
    slot.batch_preambles.push_back(std::move(preamble));
  }
  auto next_image_states = image_states_;
  std::size_t final_access_count{};
  for (const auto* recorder : recorders)
    final_access_count += recorder->final_image_accesses().size();
  next_image_states.reserve(next_image_states.size() + final_access_count);
  std::vector<VkCommandBufferSubmitInfo> command_infos;
  command_infos.reserve(recorders.size() * 2);
  std::vector<VkSubmitInfo2> submit_infos;
  submit_infos.reserve(recorders.size());
  for (std::size_t recorder_index = 0; recorder_index < recorders.size(); ++recorder_index) {
    auto& recorder = *recorders[recorder_index];
    std::vector<VkImageMemoryBarrier2> image_barriers;
    image_barriers.reserve(recorder.initial_image_accesses().size());
    for (const auto& destination : recorder.initial_image_accesses()) {
      const auto previous =
          std::find_if(next_image_states.begin(), next_image_states.end(), [&](const auto& state) {
            return same_image_subresource(state, destination);
          });
      if (previous == next_image_states.end() && destination.preserve_content)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = previous == next_image_states.end()
                                 ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
                                 : previous->stages;
      barrier.srcAccessMask = previous == next_image_states.end() ? 0 : previous->access;
      barrier.dstStageMask = destination.stages;
      barrier.dstAccessMask = destination.access;
      barrier.oldLayout =
          previous == next_image_states.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
      barrier.newLayout = destination.layout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = destination.image;
      barrier.subresourceRange = destination.range;
      image_barriers.push_back(barrier);
    }
    const auto first_command = command_infos.size();
    if (!image_barriers.empty()) {
      auto& preamble = *slot.batch_preambles[recorder_index];
      if (preamble.state() == command_recorder_state::executable) {
        result = preamble.reset(device_);
        if (result != GRANIT_SUCCESS)
          return observe_device_result(result);
      }
      result = preamble.begin(device_);
      if (result == GRANIT_SUCCESS)
        result = preamble.record_image_barriers(device_, image_barriers);
      if (result == GRANIT_SUCCESS)
        result = preamble.end(device_);
      if (result != GRANIT_SUCCESS)
        return observe_device_result(result);
      VkCommandBufferSubmitInfo preamble_info{};
      preamble_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      preamble_info.commandBuffer = preamble.native_handle();
      command_infos.push_back(preamble_info);
    }
    VkCommandBufferSubmitInfo recorder_info{};
    recorder_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    recorder_info.commandBuffer = recorder.native_handle();
    command_infos.push_back(recorder_info);
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount =
        static_cast<std::uint32_t>(command_infos.size() - first_command);
    submit_info.pCommandBufferInfos = command_infos.data() + first_command;
    submit_infos.push_back(submit_info);
    for (const auto& final : recorder.final_image_accesses()) {
      const auto state =
          std::find_if(next_image_states.begin(), next_image_states.end(),
                       [&](const auto& current) { return same_image_subresource(current, final); });
      if (state == next_image_states.end())
        next_image_states.push_back(final);
      else
        *state = final;
    }
  }
  slot.recorders.assign(recorders.begin(), recorders.end());
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS) {
    slot.recorders.clear();
    return observe_device_result(result);
  }
  const auto submit_result = device_.functions().vkQueueSubmit2(
      device_.graphics_queue(), static_cast<std::uint32_t>(submit_infos.size()),
      submit_infos.data(), slot.context->completion_fence());
  if (submit_result != VK_SUCCESS) {
    slot.recorders.clear();
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(submit_result));
  }
  for (auto* recorder : recorders)
    static_cast<void>(recorder->mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  image_states_ = std::move(next_image_states);
  slot.serial = serial;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result renderer_state::acquire_swapchain_frame(backend_swapchain_resource& resource,
                                                      std::uint32_t& image_index,
                                                      std::size_t& slot_index,
                                                      bool& needs_recreate) {
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
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

granit_result renderer_state::submit_swapchain_frame(backend_command_recorder_resource& command,
                                                     backend_swapchain_resource& resource,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     submission_serial& submitted_serial) {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(command).native();
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
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
                     [&](const auto& state) { return same_image_subresource(state, destination); });
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
                     [&](const auto& current) { return same_image_subresource(current, access); });
    if (state == image_states_.end())
      image_states_.push_back(access);
    else
      *state = access;
  }
  auto present_state = *final;
  present_state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_state.stages = VK_PIPELINE_STAGE_2_NONE;
  present_state.access = 0;
  const auto state =
      std::find_if(image_states_.begin(), image_states_.end(), [&](const auto& current) {
        return same_image_subresource(current, present_state);
      });
  *state = present_state;
  slot.recorders.clear();
  slot.recorders.push_back(&recorder);
  slot.serial = serial;
  slot.acquired = false;
  slot.awaiting_present = true;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result renderer_state::present_swapchain_frame(backend_swapchain_resource& resource,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index,
                                                      bool& needs_recreate) {
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
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

granit_result renderer_state::cancel_swapchain_frame(backend_swapchain_resource& resource,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index, bool& needs_recreate) {
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
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

granit_result renderer_state::observe_device_result(granit_result result,
                                                    const std::source_location& location) noexcept {
  bool first_loss = false;
  const auto observed = device_status_.observe(result, &first_loss);
  if (observed == GRANIT_ERROR_DEVICE_LOST)
    lifecycle_.mark_device_lost();
  if (first_loss) {
    std::array<char, 768> message{};
    const auto written = std::snprintf(
        message.data(), message.size(),
        "首次检测到 Device Lost：operation=%s, result=GRANIT_ERROR_DEVICE_LOST, "
        "backend=Vulkan/VK_ERROR_DEVICE_LOST, validation=%s, renderer_domain=%u",
        location.function_name(), validation_enabled_ ? "enabled" : "disabled", domain_);
    const auto length = written <= 0
                            ? std::size_t{0}
                            : std::min(static_cast<std::size_t>(written), message.size() - 1);
    diagnostics_.emit(diagnostic_severity::error, diagnostic_category::device,
                      std::string_view{message.data(), length});
  }
  return observed;
}

granit_result
renderer_state::wait_command_recorder(backend_command_recorder_resource& resource) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (recorder.state() != command_recorder_state::pending) {
    return GRANIT_SUCCESS;
  }
  for (auto& slot : frame_slots_) {
    if (std::find(slot.recorders.begin(), slot.recorders.end(), &recorder) !=
        slot.recorders.end()) {
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

granit_result renderer_state::create_timestamp_query_pool(
    std::uint32_t query_count,
    std::unique_ptr<backend_timestamp_query_pool_resource>& pool) noexcept {
  pool.reset();
  try {
    auto resource = std::make_unique<vulkan_timestamp_query_pool_resource>(shared_from_this());
    const auto result = resource->native().initialize(device_, query_count);
    if (result != GRANIT_SUCCESS)
      return result;
    pool = std::move(resource);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
renderer_state::read_timestamp_query_results(backend_timestamp_query_pool_resource& resource,
                                             std::uint32_t first,
                                             std::span<std::uint64_t> values) noexcept {
  auto& pool = static_cast<vulkan_timestamp_query_pool_resource&>(resource).native();
  return pool.read_nanoseconds(device_, first, values, false);
}

granit_result
renderer_state::reset_timestamp_queries(backend_command_recorder_resource& recorder_resource,
                                        backend_timestamp_query_pool_resource& pool_resource,
                                        std::uint32_t first, std::uint32_t count) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  auto& pool = static_cast<vulkan_timestamp_query_pool_resource&>(pool_resource).native();
  return pool.reset(device_, recorder.native_handle(), first, count);
}

granit_result renderer_state::write_timestamp(backend_command_recorder_resource& recorder_resource,
                                              backend_timestamp_query_pool_resource& pool_resource,
                                              granit_timestamp_stage stage,
                                              std::uint32_t index) noexcept {
  const auto native_stage =
      stage == GRANIT_TIMESTAMP_STAGE_TOP      ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
      : stage == GRANIT_TIMESTAMP_STAGE_DRAW   ? VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
      : stage == GRANIT_TIMESTAMP_STAGE_BOTTOM ? VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
                                               : VkPipelineStageFlags2{};
  if (native_stage == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  auto& pool = static_cast<vulkan_timestamp_query_pool_resource&>(pool_resource).native();
  return pool.write(device_, recorder.native_handle(), native_stage, index);
}

granit_result
renderer_state::set_timestamp_query_pool_name(backend_timestamp_query_pool_resource& resource,
                                              std::string_view name) noexcept {
  const auto native =
      static_cast<vulkan_timestamp_query_pool_resource&>(resource).native().native_handle();
  return set_object_name(VK_OBJECT_TYPE_QUERY_POOL, object_handle_value(native), name);
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

void renderer_state::destroy_native_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  static_cast<vulkan_command_recorder_resource&>(resource).native().destroy(device_);
}

} // namespace granit::detail
