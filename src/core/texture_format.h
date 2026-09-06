// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_TEXTURE_FORMAT_H_
#define GRANIT_CORE_TEXTURE_FORMAT_H_

#include <granit/renderer/resource_types.h>

namespace granit::detail {

struct texture_format_block_info {
  uint32_t width;
  uint32_t height;
  uint32_t bytes;
};

[[nodiscard]] constexpr texture_format_block_info
texture_format_block(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_SRGB:
    return {4, 4, 8};
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_SRGB:
  case GRANIT_TEXTURE_FORMAT_BC5_RG_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM:
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_SRGB:
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_UNORM:
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_SRGB:
    return {4, 4, 16};
  default:
    break;
  }
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return {1, 1, 1};
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
  case GRANIT_TEXTURE_FORMAT_D16_UNORM:
    return {1, 1, 2};
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_BGRA8_UNORM:
  case GRANIT_TEXTURE_FORMAT_BGRA8_SRGB:
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
  case GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
    return {1, 1, 4};
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT:
    return {1, 1, 8};
  default:
    return {};
  }
}

[[nodiscard]] constexpr uint32_t
texture_format_bytes_per_block(granit_texture_format format) noexcept {
  return texture_format_block(format).bytes;
}

[[nodiscard]] constexpr bool compressed_texture_format(granit_texture_format format) noexcept {
  return texture_format_block(format).width > 1;
}

[[nodiscard]] constexpr bool depth_stencil_texture_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_D16_UNORM &&
         format <= GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
}

} // namespace granit::detail

#endif
