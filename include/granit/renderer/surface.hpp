// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SURFACE_HPP_
#define GRANIT_SURFACE_HPP_

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/surface.h>

namespace granit {

/** 统一 Surface 描述构造器；原生窗口来源必须覆盖 Surface 生命周期。 */
class surface_desc {
public:
  [[nodiscard]] static surface_desc win32(void* instance, void* window) noexcept {
    surface_desc result;
    result.native_.surface_type = GRANIT_SURFACE_TYPE_WIN32_BIT;
    result.native_.source.win32 = {instance, window};
    return result;
  }

  [[nodiscard]] static surface_desc xcb(void* connection, std::uint32_t window) noexcept {
    surface_desc result;
    result.native_.surface_type = GRANIT_SURFACE_TYPE_XCB_BIT;
    result.native_.source.xcb = {connection, window, 0};
    return result;
  }

  [[nodiscard]] static surface_desc wayland(void* display, void* surface) noexcept {
    surface_desc result;
    result.native_.surface_type = GRANIT_SURFACE_TYPE_WAYLAND_BIT;
    result.native_.source.wayland = {display, surface};
    return result;
  }

  [[nodiscard]] static surface_desc canvas(std::string_view selector = "#canvas") noexcept {
    surface_desc result;
    result.native_.surface_type = GRANIT_SURFACE_TYPE_CANVAS_BIT;
    if (selector.size() <= std::numeric_limits<std::uint32_t>::max()) {
      result.native_.source.canvas = {selector.data(), static_cast<std::uint32_t>(selector.size()),
                                      0};
    } else {
      result.native_.source.canvas = {selector.data(), UINT32_MAX, 0};
    }
    return result;
  }

  [[nodiscard]] const granit_surface_desc& native() const noexcept { return native_; }

private:
  surface_desc() noexcept : native_(GRANIT_SURFACE_DESC_INIT) {}

  granit_surface_desc native_;
};

/** 无异常、move-only 的 Surface RAII 包装。 */
class surface {
public:
  surface() = default;
  ~surface() { static_cast<void>(reset()); }

  surface(const surface&) = delete;
  surface& operator=(const surface&) = delete;

  surface(surface&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}

  surface& operator=(surface&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer, const surface_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const auto native_result = granit_surface_create(renderer, &desc.native(), &handle_);
    if (native_result == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(native_result);
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto value = granit_surface_destroy(renderer_, handle_);
    if (value == GRANIT_SUCCESS || value == GRANIT_ERROR_INVALID_HANDLE) {
      renderer_ = GRANIT_NULL_HANDLE;
      handle_ = GRANIT_NULL_HANDLE;
    }
    return from_native(value);
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_surface native_handle() const noexcept { return handle_; }
  [[nodiscard]] granit_renderer renderer_handle() const noexcept { return renderer_; }

private:
  friend class window;

  void adopt(granit_renderer renderer, granit_surface handle) noexcept {
    renderer_ = renderer;
    handle_ = handle;
  }

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_surface handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
