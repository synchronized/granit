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

vulkan_renderer_state::~vulkan_renderer_state() {
  static_cast<void>(wait_for_all_submissions());
  for (auto& slot : readback_slots_)
    slot.context->destroy(device_, memory_allocator_);
  readback_slots_.clear();
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

granit_result vulkan_renderer_state::initialize(std::string_view application_name,
                                                bool enable_validation, std::uint32_t surface_types,
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
      .framebuffer_sample_counts = static_cast<std::uint32_t>(limits.framebufferColorSampleCounts &
                                                              limits.framebufferDepthSampleCounts),
      .max_sampler_anisotropy =
          device_.sampler_anisotropy_supported() ? limits.maxSamplerAnisotropy : 1.0F,
      .renderer_features = GRANIT_RENDERER_FEATURE_TIMESTAMP_QUERY_BIT |
                           GRANIT_RENDERER_FEATURE_ASYNC_READBACK_BIT |
                           GRANIT_RENDERER_FEATURE_PIPELINE_WARMUP_BIT |
                           GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT,
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
  try {
    readback_slots_.reserve(frames_in_flight);
    for (std::uint32_t index = 0; index < frames_in_flight; ++index) {
      readback_slot slot{.context = std::make_unique<vulkan_readback_context>()};
      const auto readback_result = slot.context->initialize(device_);
      if (readback_result != GRANIT_SUCCESS) {
        for (auto& initialized : readback_slots_)
          initialized.context->destroy(device_, memory_allocator_);
        readback_slots_.clear();
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
        return readback_result;
      }
      readback_slots_.push_back(std::move(slot));
    }
  } catch (...) {
    for (auto& slot : readback_slots_)
      slot.context->destroy(device_, memory_allocator_);
    readback_slots_.clear();
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
    for (auto& slot : readback_slots_)
      slot.context->destroy(device_, memory_allocator_);
    readback_slots_.clear();
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

backend_texture_format_capabilities
vulkan_renderer_state::texture_format_capabilities(granit_texture_format format) const noexcept {
  backend_texture_format_capabilities result{};
  const auto native = map_texture_format(format);
  if (native == VK_FORMAT_UNDEFINED || !instance_.valid() || !device_.valid())
    return result;
  VkFormatProperties2 properties{};
  properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
  instance_.functions().vkGetPhysicalDeviceFormatProperties2(device_.physical_device(), native,
                                                             &properties);
  const auto features = properties.formatProperties.optimalTilingFeatures;
  if ((features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0)
    result.supported_usage |= GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  if ((features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0)
    result.supported_usage |= GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  if ((features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
    result.supported_usage |= GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  if ((features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0)
    result.supported_usage |= GRANIT_TEXTURE_USAGE_STORAGE_BIT;
  if ((features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0)
    result.supported_usage |= GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  if ((features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    result.supported_usage |= GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  if ((features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0)
    result.features |= GRANIT_TEXTURE_FORMAT_FEATURE_FILTERABLE_BIT;
  result.sample_counts =
      (result.supported_usage & (GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                 GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0
          ? capabilities_.framebuffer_sample_counts
          : GRANIT_SAMPLE_COUNT_1;
  return result;
}

granit_result vulkan_renderer_state::import_pipeline_cache(const void* data,
                                                           std::uint64_t size) noexcept {
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

granit_result vulkan_renderer_state::export_pipeline_cache(void* data,
                                                           std::uint64_t& size) noexcept {
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

granit_result vulkan_renderer_state::set_object_name(VkObjectType type, std::uint64_t object,
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

granit_result vulkan_renderer_state::set_backend_resource_name(backend_resource& resource,
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

granit_result
vulkan_renderer_state::observe_device_result(granit_result result,
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

} // namespace granit::detail
