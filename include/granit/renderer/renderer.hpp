// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_HPP_
#define GRANIT_RENDERER_HPP_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include <granit/core/diagnostic.hpp>
#include <granit/core/result.hpp>
#include <granit/renderer/renderer.h>

namespace granit {

enum class surface_type : std::uint32_t {
  none = 0,
  win32 = GRANIT_SURFACE_TYPE_WIN32_BIT,
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
    };
    return from_native(granit_renderer_create(&native_desc, &handle_));
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_renderer_destroy(handle));
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
