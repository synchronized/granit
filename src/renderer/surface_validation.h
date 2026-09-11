// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_SURFACE_VALIDATION_H_
#define GRANIT_RENDERER_SURFACE_VALIDATION_H_

#include <cstdint>
#include <cstring>
#include <string_view>

#include <granit/renderer/surface.h>

namespace granit::detail {

inline constexpr std::uint32_t maximum_canvas_selector_length = 4096;
inline constexpr std::string_view default_canvas_selector = "#canvas";

[[nodiscard]] inline granit_result validate_surface_desc(const granit_surface_desc* desc) noexcept {
  if (desc == nullptr || desc->struct_size < GRANIT_SURFACE_DESC_VERSION_1_SIZE ||
      desc->flags != 0 || desc->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  switch (desc->surface_type) {
  case GRANIT_SURFACE_TYPE_WIN32_BIT:
    return desc->source.win32.instance != nullptr && desc->source.win32.window != nullptr
               ? GRANIT_SUCCESS
               : GRANIT_ERROR_INVALID_ARGUMENT;
  case GRANIT_SURFACE_TYPE_XCB_BIT:
    return desc->source.xcb.connection != nullptr && desc->source.xcb.window != 0 &&
                   desc->source.xcb.reserved == 0
               ? GRANIT_SUCCESS
               : GRANIT_ERROR_INVALID_ARGUMENT;
  case GRANIT_SURFACE_TYPE_WAYLAND_BIT:
    return desc->source.wayland.display != nullptr && desc->source.wayland.surface != nullptr
               ? GRANIT_SUCCESS
               : GRANIT_ERROR_INVALID_ARGUMENT;
  case GRANIT_SURFACE_TYPE_CANVAS_BIT: {
    const auto& canvas = desc->source.canvas;
    return canvas.reserved == 0 && canvas.selector_length <= maximum_canvas_selector_length &&
                   (canvas.selector != nullptr || canvas.selector_length == 0) &&
                   (canvas.selector == nullptr ||
                    (canvas.selector_length != 0 &&
                     std::memchr(canvas.selector, '\0', canvas.selector_length) == nullptr))
               ? GRANIT_SUCCESS
               : GRANIT_ERROR_INVALID_ARGUMENT;
  }
  default:
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
}

} // namespace granit::detail

#endif
