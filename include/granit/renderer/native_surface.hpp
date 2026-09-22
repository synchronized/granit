// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_NATIVE_SURFACE_HPP_
#define GRANIT_RENDERER_NATIVE_SURFACE_HPP_

#include <cstdint>
#include <limits>
#include <string_view>

#include <granit/renderer/native_surface.h>
#include <granit/renderer/surface.hpp>

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

inline result surface::initialize(renderer& owner, const surface_desc& desc) noexcept {
  if (valid())
    return result::invalid_argument;
  granit_surface handle = GRANIT_NULL_HANDLE;
  const auto value = granit_surface_create(owner.native_handle(), &desc.native(), &handle);
  if (value == GRANIT_SUCCESS)
    adopt(owner.native_handle(), handle);
  return from_native(value);
}

} // namespace granit

#endif
