// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESOURCE_TYPES_HPP_
#define GRANIT_RESOURCE_TYPES_HPP_

#include <cstdint>

#include <granit/resource_types.h>

namespace granit {

enum class memory_location : std::uint32_t {
  automatic = GRANIT_MEMORY_LOCATION_AUTOMATIC,
  device = GRANIT_MEMORY_LOCATION_DEVICE,
  upload = GRANIT_MEMORY_LOCATION_UPLOAD,
  readback = GRANIT_MEMORY_LOCATION_READBACK,
};

enum class buffer_usage : std::uint32_t {
  transfer_source = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT,
  transfer_destination = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,
  vertex = GRANIT_BUFFER_USAGE_VERTEX_BIT,
  index = GRANIT_BUFFER_USAGE_INDEX_BIT,
  uniform = GRANIT_BUFFER_USAGE_UNIFORM_BIT,
  storage = GRANIT_BUFFER_USAGE_STORAGE_BIT,
  indirect = GRANIT_BUFFER_USAGE_INDIRECT_BIT,
};

[[nodiscard]] constexpr buffer_usage operator|(buffer_usage left, buffer_usage right) noexcept {
  return static_cast<buffer_usage>(static_cast<std::uint32_t>(left) |
                                   static_cast<std::uint32_t>(right));
}

enum class texture_dimension : std::uint32_t {
  one_dimensional = GRANIT_TEXTURE_DIMENSION_1D,
  two_dimensional = GRANIT_TEXTURE_DIMENSION_2D,
  three_dimensional = GRANIT_TEXTURE_DIMENSION_3D,
  cube = GRANIT_TEXTURE_DIMENSION_CUBE,
};

enum class texture_usage : std::uint32_t {
  transfer_source = GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT,
  transfer_destination = GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT,
  sampled = GRANIT_TEXTURE_USAGE_SAMPLED_BIT,
  storage = GRANIT_TEXTURE_USAGE_STORAGE_BIT,
  color_attachment = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
  depth_stencil_attachment = GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
};

[[nodiscard]] constexpr texture_usage operator|(texture_usage left, texture_usage right) noexcept {
  return static_cast<texture_usage>(static_cast<std::uint32_t>(left) |
                                    static_cast<std::uint32_t>(right));
}

enum class texture_format : std::uint32_t {
  undefined = GRANIT_TEXTURE_FORMAT_UNDEFINED,
  r8_unorm = GRANIT_TEXTURE_FORMAT_R8_UNORM,
  rg8_unorm = GRANIT_TEXTURE_FORMAT_RG8_UNORM,
  rgba8_unorm = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
  rgba8_srgb = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB,
  bgra8_unorm = GRANIT_TEXTURE_FORMAT_BGRA8_UNORM,
  bgra8_srgb = GRANIT_TEXTURE_FORMAT_BGRA8_SRGB,
  rgba16_float = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
  d16_unorm = GRANIT_TEXTURE_FORMAT_D16_UNORM,
  d32_float = GRANIT_TEXTURE_FORMAT_D32_FLOAT,
  d24_unorm_s8_uint = GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT,
  d32_float_s8_uint = GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT,
};

enum class sample_count : std::uint32_t {
  one = GRANIT_SAMPLE_COUNT_1,
  two = GRANIT_SAMPLE_COUNT_2,
  four = GRANIT_SAMPLE_COUNT_4,
  eight = GRANIT_SAMPLE_COUNT_8,
};

enum class texture_aspect : std::uint32_t {
  automatic = GRANIT_TEXTURE_ASPECT_AUTOMATIC,
  color = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
  depth = GRANIT_TEXTURE_ASPECT_DEPTH_BIT,
  stencil = GRANIT_TEXTURE_ASPECT_STENCIL_BIT,
};

[[nodiscard]] constexpr texture_aspect operator|(texture_aspect left,
                                                 texture_aspect right) noexcept {
  return static_cast<texture_aspect>(static_cast<std::uint32_t>(left) |
                                     static_cast<std::uint32_t>(right));
}

enum class filter : std::uint32_t {
  nearest = GRANIT_FILTER_NEAREST,
  linear = GRANIT_FILTER_LINEAR,
};

enum class mipmap_filter : std::uint32_t {
  nearest = GRANIT_MIPMAP_FILTER_NEAREST,
  linear = GRANIT_MIPMAP_FILTER_LINEAR,
};

enum class address_mode : std::uint32_t {
  repeat = GRANIT_ADDRESS_MODE_REPEAT,
  mirrored_repeat = GRANIT_ADDRESS_MODE_MIRRORED_REPEAT,
  clamp_to_edge = GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE,
};

enum class compare_operation : std::uint32_t {
  disabled = GRANIT_COMPARE_OPERATION_DISABLED,
  never = GRANIT_COMPARE_OPERATION_NEVER,
  less = GRANIT_COMPARE_OPERATION_LESS,
  equal = GRANIT_COMPARE_OPERATION_EQUAL,
  less_equal = GRANIT_COMPARE_OPERATION_LESS_EQUAL,
  greater = GRANIT_COMPARE_OPERATION_GREATER,
  not_equal = GRANIT_COMPARE_OPERATION_NOT_EQUAL,
  greater_equal = GRANIT_COMPARE_OPERATION_GREATER_EQUAL,
  always = GRANIT_COMPARE_OPERATION_ALWAYS,
};

} // namespace granit

#endif
