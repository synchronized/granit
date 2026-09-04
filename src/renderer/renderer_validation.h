// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_VALIDATION_H_
#define GRANIT_RENDERER_RENDERER_VALIDATION_H_

#include <cstdint>
#include <cstring>

#include <granit/renderer/renderer.h>

namespace granit::detail {

inline constexpr std::uint32_t maximum_renderer_application_name_length = 4096;
inline constexpr std::uint32_t maximum_renderer_object_name_length = 4096;
[[nodiscard]] inline granit_result
validate_renderer_desc(const granit_renderer_desc& desc) noexcept {
  constexpr std::uint32_t supported_flags = GRANIT_RENDERER_ENABLE_VALIDATION_BIT;
  constexpr std::uint32_t supported_surface_types =
      GRANIT_SURFACE_TYPE_WIN32_BIT | GRANIT_SURFACE_TYPE_XCB_BIT |
      GRANIT_SURFACE_TYPE_WAYLAND_BIT | GRANIT_SURFACE_TYPE_CANVAS_BIT;
  if (desc.struct_size < GRANIT_RENDERER_DESC_SIZE ||
      desc.api_version != GRANIT_RENDERER_API_VERSION_CURRENT ||
      (desc.flags & ~supported_flags) != 0 ||
      desc.application_name_length > maximum_renderer_application_name_length ||
      (desc.surface_types & ~supported_surface_types) != 0 || desc.reserved != 0 ||
      desc.frames_in_flight == 0 || desc.frames_in_flight > GRANIT_MAX_FRAMES_IN_FLIGHT ||
      (desc.diagnostic_callback == nullptr && desc.diagnostic_user_data != nullptr) ||
      desc.backend > GRANIT_RENDERER_BACKEND_WEBGPU) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.application_name == nullptr) {
    return desc.application_name_length == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.application_name_length == 0 ||
      std::memchr(desc.application_name, '\0', desc.application_name_length) != nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::detail

#endif
