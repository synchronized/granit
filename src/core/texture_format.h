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

[[nodiscard]] constexpr bool
texture_region_has_valid_block_alignment(granit_texture_format format, uint32_t x, uint32_t y,
                                         uint32_t width, uint32_t height, uint32_t mip_width,
                                         uint32_t mip_height) noexcept {
  const auto block = texture_format_block(format);
  return block.bytes != 0 && x % block.width == 0 && y % block.height == 0 &&
         (width % block.width == 0 || x + width == mip_width) &&
         (height % block.height == 0 || y + height == mip_height);
}

struct texture_transfer_footprint {
  uint64_t tight_row{};
  uint64_t row_pitch{};
  uint64_t block_rows{};
  uint64_t image_rows{};
  uint64_t required_size{};
};

[[nodiscard]] constexpr bool
calculate_texture_transfer_footprint(granit_texture_format format, uint32_t width, uint32_t height,
                                     uint64_t image_count, const granit_texture_data_layout& layout,
                                     texture_transfer_footprint& result) noexcept {
  const auto block = texture_format_block(format);
  if (block.bytes == 0 || width == 0 || height == 0 || image_count == 0)
    return false;
  const uint64_t columns = (uint64_t{width} + block.width - 1) / block.width;
  const uint64_t rows = (uint64_t{height} + block.height - 1) / block.height;
  if (columns > UINT64_MAX / block.bytes)
    return false;
  const uint64_t tight_row = columns * block.bytes;
  const uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const uint64_t image_rows = layout.rows_per_image == 0 ? rows : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % block.bytes != 0 || image_rows < rows ||
      image_rows > UINT64_MAX / row_pitch)
    return false;
  const uint64_t image_pitch = image_rows * row_pitch;
  if (image_count - 1 > UINT64_MAX / image_pitch || rows - 1 > UINT64_MAX / row_pitch)
    return false;
  const uint64_t prior_images = (image_count - 1) * image_pitch;
  const uint64_t final_rows = (rows - 1) * row_pitch;
  if (prior_images > UINT64_MAX - final_rows || prior_images + final_rows > UINT64_MAX - tight_row)
    return false;
  result = {tight_row, row_pitch, rows, image_rows, prior_images + final_rows + tight_row};
  return true;
}

} // namespace granit::detail

#endif
