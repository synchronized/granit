// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

namespace {

std::uint32_t to_context_surface_types(std::uint32_t surface_types) noexcept {
  std::uint32_t result{};
  if ((surface_types & GRANIT_SURFACE_TYPE_WIN32_BIT) != 0)
    result |= GRANIT_WEBGPU_SURFACE_TYPE_WIN32_BIT;
  if ((surface_types & GRANIT_SURFACE_TYPE_XCB_BIT) != 0)
    result |= GRANIT_WEBGPU_SURFACE_TYPE_XCB_BIT;
  if ((surface_types & GRANIT_SURFACE_TYPE_WAYLAND_BIT) != 0)
    result |= GRANIT_WEBGPU_SURFACE_TYPE_WAYLAND_BIT;
  if ((surface_types & GRANIT_SURFACE_TYPE_CANVAS_BIT) != 0)
    result |= GRANIT_WEBGPU_SURFACE_TYPE_CANVAS_BIT;
  return result;
}

} // namespace

backend_texture_format_capabilities
webgpu_renderer_state::texture_format_capabilities(granit_texture_format format) const noexcept {
  backend_texture_format_capabilities result{};
  std::uint32_t required_compression{};
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_SRGB:
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_SRGB:
  case GRANIT_TEXTURE_FORMAT_BC5_RG_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_SRGB:
    required_compression = GRANIT_WEBGPU_TEXTURE_COMPRESSION_BC_BIT;
    break;
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_SRGB:
    required_compression = GRANIT_WEBGPU_TEXTURE_COMPRESSION_ETC2_BIT;
    break;
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_UNORM:
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_SRGB:
    required_compression = GRANIT_WEBGPU_TEXTURE_COMPRESSION_ASTC_BIT;
    break;
  default:
    break;
  }
  if (required_compression != 0) {
    if ((capabilities_.texture_compression_features & required_compression) == 0)
      return result;
    result.supported_usage = GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT |
                             GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT |
                             GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
    result.features = GRANIT_TEXTURE_FORMAT_FEATURE_FILTERABLE_BIT;
    result.sample_counts = GRANIT_SAMPLE_COUNT_1;
    return result;
  }
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    result.supported_usage =
        GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT |
        GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
    result.features = GRANIT_TEXTURE_FORMAT_FEATURE_FILTERABLE_BIT;
    result.sample_counts = capabilities_.framebuffer_sample_counts;
    break;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    result.supported_usage =
        GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT |
        GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    result.sample_counts = capabilities_.framebuffer_sample_counts;
    break;
  default:
    break;
  }
  return result;
}

webgpu_renderer_state::~webgpu_renderer_state() {
  presentation_.reset();
  resource_owner_.reset();
  command_owner_.reset();
  pipeline_owner_.reset();
  if (instance_ != 0) {
    static_cast<void>(context_.destroy_instance(instance_));
    instance_ = 0;
  }
  context_.close();
}

void* webgpu_renderer_state::allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void webgpu_renderer_state::deallocate(void* memory, std::uint64_t, std::uint64_t alignment,
                                       void*) noexcept {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void webgpu_renderer_state::diagnose(granit_diagnostic_severity severity,
                                     granit_diagnostic_category category, const char* message,
                                     std::uint32_t message_length, void* user_data) noexcept {
  if (user_data == nullptr || (message == nullptr && message_length != 0)) {
    return;
  }
  auto& state = *static_cast<webgpu_renderer_state*>(user_data);
  if (state.diagnostic_callback_ == nullptr) {
    return;
  }
  try {
    state.diagnostic_callback_(severity, category, message, message_length,
                               state.diagnostic_user_data_);
  } catch (...) {
  }
}

granit_result
webgpu_renderer_state::initialize_static(std::uint32_t surface_types,
                                         granit_diagnostic_callback diagnostic_callback,
                                         void* diagnostic_user_data) noexcept {
  if (instance_ != 0 || context_.is_open()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  diagnostic_callback_ = diagnostic_callback;
  diagnostic_user_data_ = diagnostic_user_data;
  surface_types_ = surface_types;
  auto result = context_.open();
  if (result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, result};
    return result;
  }
  return finish_initialization();
}

granit_result webgpu_renderer_state::finish_initialization() noexcept {
  webgpu_host_api host{sizeof(host), 0, diagnose, this, allocate, deallocate, nullptr};
  auto result = context_.create_instance(&host, &instance_);
  if (result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, result};
    context_.close();
    return result;
  }
  const auto refresh_result = refresh_state();
  return refresh_result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : refresh_result;
}

granit_result webgpu_renderer_state::process_backend_events() noexcept {
  if (instance_ == 0) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto result = context_.process_events(instance_);
  if (result != GRANIT_SUCCESS && result != GRANIT_ERROR_NOT_READY &&
      result != GRANIT_ERROR_DEVICE_LOST) {
    return result;
  }
  const auto refresh_result = refresh_state();
  return refresh_result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : refresh_result;
}

backend_lifecycle_status webgpu_renderer_state::lifecycle_status() const noexcept {
  return lifecycle_;
}

granit_result webgpu_renderer_state::refresh_state() noexcept {
  webgpu_instance_status status{};
  status.struct_size = sizeof(status);
  const auto status_result = context_.get_instance_status(instance_, &status);
  if (status_result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, status_result};
    return status_result;
  }
  switch (status.state) {
  case GRANIT_WEBGPU_INSTANCE_STATE_INITIALIZING:
    lifecycle_ = {backend_lifecycle_state::initializing, GRANIT_SUCCESS};
    return GRANIT_ERROR_NOT_READY;
  case GRANIT_WEBGPU_INSTANCE_STATE_FAILED:
    lifecycle_ = {backend_lifecycle_state::failed, status.failure_result};
    return status.failure_result;
  case GRANIT_WEBGPU_INSTANCE_STATE_DEVICE_LOST:
    lifecycle_ = {backend_lifecycle_state::device_lost, status.failure_result};
    return status.failure_result;
  case GRANIT_WEBGPU_INSTANCE_STATE_READY:
    break;
  default:
    lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_INTERNAL};
    return GRANIT_ERROR_INTERNAL;
  }

  if (presentation_ == nullptr || resource_owner_ == nullptr || pipeline_owner_ == nullptr ||
      command_owner_ == nullptr) {
    webgpu_capabilities capabilities{};
    capabilities.struct_size = sizeof(capabilities);
    const auto capabilities_result = context_.get_capabilities(instance_, &capabilities);
    if (capabilities_result != GRANIT_SUCCESS) {
      lifecycle_ = {backend_lifecycle_state::failed, capabilities_result};
      return capabilities_result;
    }
    capabilities_ = {
        capabilities.uniform_buffer_offset_alignment,
        capabilities.storage_buffer_offset_alignment,
        capabilities.max_uniform_buffer_binding_size,
        capabilities.max_storage_buffer_binding_size,
        capabilities.framebuffer_sample_counts,
        capabilities.max_sampler_anisotropy,
        ((capabilities.renderer_features & GRANIT_WEBGPU_FEATURE_TIMESTAMP_QUERY_BIT) != 0
             ? GRANIT_RENDERER_FEATURE_TIMESTAMP_QUERY_BIT
             : UINT64_C(0)) |
            GRANIT_RENDERER_FEATURE_ASYNC_READBACK_BIT |
            GRANIT_RENDERER_FEATURE_PIPELINE_WARMUP_BIT |
            GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT,
    };
    capabilities_.texture_compression_features = capabilities.texture_compression_features;
    context_surface_types_ = capabilities.surface_types;
    if ((to_context_surface_types(surface_types_) & ~context_surface_types_) != 0) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_UNSUPPORTED};
      return GRANIT_ERROR_UNSUPPORTED;
    }
    try {
      auto presentation = std::make_unique<webgpu_presentation_adapter>(context_, instance_);
      auto resource_owner =
          std::make_shared<webgpu_resource_owner>(webgpu_resource_owner{&context_, instance_});
      auto pipeline_owner =
          std::make_shared<webgpu_pipeline_owner>(webgpu_pipeline_owner{&context_, instance_});
      auto command_owner =
          std::make_shared<webgpu_command_owner>(webgpu_command_owner{&context_, instance_});
      presentation_ = std::move(presentation);
      resource_owner_ = std::move(resource_owner);
      pipeline_owner_ = std::move(pipeline_owner);
      command_owner_ = std::move(command_owner);
    } catch (const std::bad_alloc&) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_OUT_OF_MEMORY};
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_INTERNAL};
      return GRANIT_ERROR_INTERNAL;
    }
  }
  lifecycle_ = {backend_lifecycle_state::ready, GRANIT_SUCCESS};
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
