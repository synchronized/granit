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

[[nodiscard]] inline granit_result
validate_canvas_surface_desc(const granit_canvas_surface_desc* desc) noexcept {
  if (desc == nullptr || desc->struct_size < GRANIT_CANVAS_SURFACE_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->selector_length > maximum_canvas_selector_length ||
      (desc->selector == nullptr && desc->selector_length != 0) ||
      (desc->selector != nullptr &&
       (desc->selector_length == 0 ||
        std::memchr(desc->selector, '\0', desc->selector_length) != nullptr))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::detail

#endif
