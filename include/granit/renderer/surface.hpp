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

struct win32_surface_desc {
  void* instance{};
  void* window{};
};

struct xcb_surface_desc {
  void* connection{};
  std::uint32_t window{};
};

struct wayland_surface_desc {
  void* display{};
  void* surface{};
};

struct canvas_surface_desc {
  std::string_view selector{"#canvas"};
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

  [[nodiscard]] result initialize_win32(granit_renderer renderer,
                                        const win32_surface_desc& desc) noexcept {
    if (valid()) {
      return result::invalid_argument;
    }
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_win32_surface_desc native_desc{
        .struct_size = sizeof(granit_win32_surface_desc),
        .instance = desc.instance,
        .window = desc.window,
    };
    const auto native_result = granit_surface_create_win32(renderer, &native_desc, &handle_);
    if (native_result == GRANIT_SUCCESS) {
      renderer_ = renderer;
    }
    return from_native(native_result);
  }

  [[nodiscard]] result initialize_xcb(granit_renderer renderer,
                                      const xcb_surface_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_xcb_surface_desc native_desc{
        .struct_size = sizeof(granit_xcb_surface_desc),
        .connection = desc.connection,
        .window = desc.window,
    };
    const auto native_result = granit_surface_create_xcb(renderer, &native_desc, &handle_);
    if (native_result == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(native_result);
  }

  [[nodiscard]] result initialize_wayland(granit_renderer renderer,
                                          const wayland_surface_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_wayland_surface_desc native_desc{
        .struct_size = sizeof(granit_wayland_surface_desc),
        .display = desc.display,
        .surface = desc.surface,
    };
    const auto native_result = granit_surface_create_wayland(renderer, &native_desc, &handle_);
    if (native_result == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(native_result);
  }

  [[nodiscard]] result initialize_canvas(granit_renderer renderer,
                                         const canvas_surface_desc& desc = {}) noexcept {
    if (valid() || desc.selector.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_canvas_surface_desc native_desc{
        .struct_size = sizeof(granit_canvas_surface_desc),
        .reserved = 0,
        .selector = desc.selector.data(),
        .selector_length = static_cast<std::uint32_t>(desc.selector.size()),
    };
    const auto native_result = granit_surface_create_canvas(renderer, &native_desc, &handle_);
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
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_surface handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
