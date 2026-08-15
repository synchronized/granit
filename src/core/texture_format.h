// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_TEXTURE_FORMAT_H_
#define GRANIT_CORE_TEXTURE_FORMAT_H_

#include <granit/renderer/resource_types.h>

namespace granit::detail {

[[nodiscard]] constexpr uint32_t texture_format_bytes_per_block(
    granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return 1;
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
  case GRANIT_TEXTURE_FORMAT_D16_UNORM:
    return 2;
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_BGRA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_BGRA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
  case GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
    return 4;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT:
    return 8;
  default:
    return 0;
  }
}

} // namespace granit::detail

#endif
