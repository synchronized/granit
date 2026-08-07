// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_HPP_
#define GRANIT_RENDERER_HPP_

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <granit/renderer.h>
#include <granit/result.hpp>

namespace granit {

struct renderer_desc {
  std::string_view application_name{"Granit Application"};
  bool enable_validation{};
};

/** 无异常、move-only 的 renderer RAII 包装。 */
class renderer {
public:
  renderer() = default;
  ~renderer() { static_cast<void>(reset()); }

  renderer(const renderer&) = delete;
  renderer& operator=(const renderer&) = delete;

  renderer(renderer&& other) noexcept
      : handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}

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

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_renderer native_handle() const noexcept { return handle_; }

private:
  granit_renderer handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
