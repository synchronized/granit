// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_HPP_
#define GRANIT_RENDERER_HPP_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <granit/core/diagnostic.hpp>
#include <granit/core/result.hpp>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.hpp>

namespace granit {

enum class surface_type : std::uint32_t {
  none = 0,
  win32 = GRANIT_SURFACE_TYPE_WIN32_BIT,
  xcb = GRANIT_SURFACE_TYPE_XCB_BIT,
  wayland = GRANIT_SURFACE_TYPE_WAYLAND_BIT,
  canvas = GRANIT_SURFACE_TYPE_CANVAS_BIT,
};

enum class renderer_backend : std::uint32_t {
  automatic = GRANIT_RENDERER_BACKEND_AUTO,
  vulkan = GRANIT_RENDERER_BACKEND_VULKAN,
  webgpu = GRANIT_RENDERER_BACKEND_WEBGPU,
};

[[nodiscard]] constexpr surface_type operator|(surface_type left, surface_type right) noexcept {
  return static_cast<surface_type>(static_cast<std::uint32_t>(left) |
                                   static_cast<std::uint32_t>(right));
}

struct renderer_desc {
  std::string_view application_name{"Granit Application"};
  bool enable_validation{};
  surface_type surface_types{surface_type::none};
  std::uint32_t frames_in_flight{GRANIT_DEFAULT_FRAMES_IN_FLIGHT};
  diagnostic_callback diagnostics{};
  void* diagnostic_user_data{};
  renderer_backend backend{renderer_backend::automatic};
};

struct renderer_info {
  renderer_backend backend{renderer_backend::automatic};
  std::string adapter_name;
  std::uint32_t vendor_id{};
  std::uint32_t device_id{};
};

struct renderer_limits {
  std::uint64_t uniform_buffer_offset_alignment{};
  std::uint64_t max_uniform_buffer_binding_size{};
  std::uint32_t framebuffer_sample_counts{};
  float max_sampler_anisotropy{1.0F};

  [[nodiscard]] constexpr bool supports_sample_count(sample_count samples) const noexcept {
    const auto value = static_cast<std::uint32_t>(samples);
    return (framebuffer_sample_counts & value) == value;
  }
};

enum class shader_feature : std::uint64_t {
  float16 = GRANIT_SHADER_FEATURE_FLOAT16_BIT,
  subgroup = GRANIT_SHADER_FEATURE_SUBGROUP_BIT,
};

struct renderer_shader_capabilities {
  renderer_backend backend{renderer_backend::automatic};
  std::uint32_t profile{GRANIT_SHADER_PROFILE_PORTABLE};
  std::uint64_t supported_features{};

  [[nodiscard]] constexpr bool supports(shader_feature feature) const noexcept {
    const auto bit = static_cast<std::uint64_t>(feature);
    return (supported_features & bit) == bit;
  }
};

struct renderer_resource_stats {
  std::uint64_t total_live_count{};
  std::uint64_t buffer_count{};
  std::uint64_t texture_count{};
  std::uint64_t texture_view_count{};
  std::uint64_t sampler_count{};
  std::uint64_t shader_count{};
  std::uint64_t bind_group_layout_count{};
  std::uint64_t bind_group_count{};
  std::uint64_t pipeline_layout_count{};
  std::uint64_t graphics_pipeline_count{};
  std::uint64_t compute_pipeline_count{};
  std::uint64_t surface_count{};
  std::uint64_t swapchain_count{};
  std::uint64_t command_recorder_count{};
  std::uint64_t frame_context_count{};
  std::uint64_t frame_count{};
  std::uint64_t timestamp_query_pool_count{};
  std::uint64_t upload_batch_count{};
  std::uint64_t pending_retirement_count{};
};

enum class renderer_state : std::uint32_t {
  initializing = GRANIT_RENDERER_STATE_INITIALIZING,
  ready = GRANIT_RENDERER_STATE_READY,
  failed = GRANIT_RENDERER_STATE_FAILED,
  device_lost = GRANIT_RENDERER_STATE_DEVICE_LOST,
};

struct renderer_status {
  renderer_state state{renderer_state::initializing};
  result failure_result{result::success};
};

/** 无异常、move-only 的 renderer RAII 包装。 */
class renderer {
public:
  renderer() = default;
  ~renderer() { static_cast<void>(reset()); }

  renderer(const renderer&) = delete;
  renderer& operator=(const renderer&) = delete;

  renderer(renderer&& other) noexcept : handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}

  renderer& operator=(renderer&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(const renderer_desc& desc = {}) noexcept {
    if (valid() || desc.application_name.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }

    const granit_renderer_desc native_desc{
        .struct_size = sizeof(granit_renderer_desc),
        .api_version = GRANIT_RENDERER_API_VERSION_CURRENT,
        .application_name = desc.application_name.data(),
        .application_name_length = static_cast<std::uint32_t>(desc.application_name.size()),
        .flags = desc.enable_validation ? GRANIT_RENDERER_ENABLE_VALIDATION_BIT : UINT32_C(0),
        .surface_types = static_cast<std::uint32_t>(desc.surface_types),
        .frames_in_flight = desc.frames_in_flight,
        .reserved = 0,
        .diagnostic_callback = desc.diagnostics,
        .diagnostic_user_data = desc.diagnostic_user_data,
        .backend = static_cast<granit_renderer_backend>(desc.backend),
    };
    return from_native(granit_renderer_create(&native_desc, &handle_));
  }

  [[nodiscard]] result get_info(renderer_info& info) const {
    granit_renderer_info native = GRANIT_RENDERER_INFO_INIT;
    auto query_result = from_native(granit_renderer_get_info(handle_, &native));
    if (query_result.failed())
      return query_result;
    std::string adapter(native.adapter_name_length, '\0');
    if (!adapter.empty()) {
      native.adapter_name = adapter.data();
      native.adapter_name_capacity = static_cast<std::uint32_t>(adapter.size() + 1);
      query_result = from_native(granit_renderer_get_info(handle_, &native));
      if (query_result.failed())
        return query_result;
    }
    info = {.backend = static_cast<renderer_backend>(native.backend),
            .adapter_name = std::move(adapter),
            .vendor_id = native.vendor_id,
            .device_id = native.device_id};
    return result::success;
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_renderer_destroy(handle));
  }

  [[nodiscard]] result set_object_name(granit_handle object, std::string_view name) const noexcept {
    if (name.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }
    return from_native(granit_renderer_set_object_name(handle_, object, name.data(),
                                                       static_cast<std::uint32_t>(name.size())));
  }

  [[nodiscard]] result get_limits(renderer_limits& limits) const noexcept {
    granit_renderer_limits native = GRANIT_RENDERER_LIMITS_INIT;
    const auto query_result = from_native(granit_renderer_get_limits(handle_, &native));
    if (query_result.failed()) {
      return query_result;
    }
    limits = {
        .uniform_buffer_offset_alignment = native.uniform_buffer_offset_alignment,
        .max_uniform_buffer_binding_size = native.max_uniform_buffer_binding_size,
        .framebuffer_sample_counts = native.framebuffer_sample_counts,
        .max_sampler_anisotropy = native.max_sampler_anisotropy,
    };
    return result::success;
  }

  [[nodiscard]] result
  get_shader_capabilities(renderer_shader_capabilities& capabilities) const noexcept {
    granit_renderer_shader_capabilities native = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
    const auto query_result =
        from_native(granit_renderer_get_shader_capabilities(handle_, &native));
    if (query_result.failed())
      return query_result;
    capabilities = {
        .backend = static_cast<renderer_backend>(native.backend),
        .profile = native.profile,
        .supported_features = native.supported_features,
    };
    return result::success;
  }

  [[nodiscard]] result get_status(renderer_status& status) const noexcept {
    granit_renderer_status native = GRANIT_RENDERER_STATUS_INIT;
    const auto query_result = from_native(granit_renderer_get_status(handle_, &native));
    if (query_result.failed()) {
      return query_result;
    }
    status = {.state = static_cast<renderer_state>(native.state),
              .failure_result = from_native(native.failure_result)};
    return result::success;
  }

  [[nodiscard]] result get_resource_stats(renderer_resource_stats& stats) const noexcept {
    granit_renderer_resource_stats native = GRANIT_RENDERER_RESOURCE_STATS_INIT;
    const auto query_result = from_native(granit_renderer_get_resource_stats(handle_, &native));
    if (query_result.failed()) {
      return query_result;
    }
    stats = {.total_live_count = native.total_live_count,
             .buffer_count = native.buffer_count,
             .texture_count = native.texture_count,
             .texture_view_count = native.texture_view_count,
             .sampler_count = native.sampler_count,
             .shader_count = native.shader_count,
             .bind_group_layout_count = native.bind_group_layout_count,
             .bind_group_count = native.bind_group_count,
             .pipeline_layout_count = native.pipeline_layout_count,
             .graphics_pipeline_count = native.graphics_pipeline_count,
             .compute_pipeline_count = native.compute_pipeline_count,
             .surface_count = native.surface_count,
             .swapchain_count = native.swapchain_count,
             .command_recorder_count = native.command_recorder_count,
             .frame_context_count = native.frame_context_count,
             .frame_count = native.frame_count,
             .timestamp_query_pool_count = native.timestamp_query_pool_count,
             .upload_batch_count = native.upload_batch_count,
             .pending_retirement_count = native.pending_retirement_count};
    return result::success;
  }

  [[nodiscard]] result process_events() noexcept {
    return from_native(granit_renderer_process_events(handle_));
  }

  [[nodiscard]] result import_pipeline_cache(std::span<const std::byte> data) noexcept {
    return from_native(granit_renderer_pipeline_cache_import(handle_, data.data(), data.size()));
  }

  [[nodiscard]] result query_pipeline_cache_size(std::uint64_t& size) const noexcept {
    size = 0;
    return from_native(granit_renderer_pipeline_cache_export(handle_, nullptr, &size));
  }

  [[nodiscard]] result export_pipeline_cache(std::span<std::byte> data,
                                             std::uint64_t& size) const noexcept {
    size = data.size();
    return from_native(granit_renderer_pipeline_cache_export(handle_, data.data(), &size));
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_renderer native_handle() const noexcept { return handle_; }

private:
  granit_renderer handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
