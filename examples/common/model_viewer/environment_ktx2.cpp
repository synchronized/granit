// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/environment_ktx2.h"

#include <algorithm>
#include <array>
#include <new>
#include <utility>

namespace granit::example::model_viewer {
namespace {

constexpr std::array<std::uint8_t, 12> identifier{0xab, 0x4b, 0x54, 0x58, 0x20, 0x32,
                                                  0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
constexpr std::size_t header_size = 80;
constexpr std::uint32_t rgba16_float_vk_format = 97;
constexpr std::uint64_t bytes_per_pixel = 8;
constexpr std::uint64_t face_count = 6;

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < 4; ++index)
    value |= std::to_integer<std::uint32_t>(bytes[offset + index]) << (index * 8U);
  return value;
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index)
    value |= std::to_integer<std::uint64_t>(bytes[offset + index]) << (index * 8U);
  return value;
}

bool range_inside(std::uint64_t offset, std::uint64_t size, std::size_t container_size) noexcept {
  return offset <= container_size && size <= container_size - offset;
}

} // namespace

environment_ktx2_error parse_environment_ktx2_cube(std::span<const std::byte> bytes,
                                                   environment_ktx2_cube& cube) {
  cube = {};
  if (bytes.size() < header_size)
    return environment_ktx2_error::truncated;
  for (std::size_t index = 0; index < identifier.size(); ++index) {
    if (std::to_integer<std::uint8_t>(bytes[index]) != identifier[index])
      return environment_ktx2_error::invalid_identifier;
  }

  const auto width = read_u32(bytes, 20);
  const auto height = read_u32(bytes, 24);
  const auto level_count = read_u32(bytes, 40);
  if (read_u32(bytes, 12) != rgba16_float_vk_format || read_u32(bytes, 16) != 2 || width == 0 ||
      width != height || (width & (width - 1U)) != 0 || read_u32(bytes, 28) != 0 ||
      read_u32(bytes, 32) > 1 || read_u32(bytes, 36) != face_count || level_count == 0 ||
      read_u32(bytes, 44) != 0 || level_count > 13 ||
      header_size + static_cast<std::size_t>(level_count) * 24 > bytes.size()) {
    return environment_ktx2_error::unsupported_layout;
  }

  try {
    cube.levels.reserve(level_count);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    ranges.reserve(level_count);
    auto resolution = width;
    const auto minimum_data_offset = header_size + static_cast<std::uint64_t>(level_count) * 24U;
    for (std::uint32_t level = 0; level < level_count; ++level) {
      const auto index_offset = header_size + static_cast<std::size_t>(level) * 24;
      const auto byte_offset = read_u64(bytes, index_offset);
      const auto byte_length = read_u64(bytes, index_offset + 8);
      const auto uncompressed_length = read_u64(bytes, index_offset + 16);
      const auto expected_length =
          static_cast<std::uint64_t>(resolution) * resolution * face_count * bytes_per_pixel;
      const bool valid_range = byte_offset >= minimum_data_offset &&
                               range_inside(byte_offset, byte_length, bytes.size());
      const auto byte_end = valid_range ? byte_offset + byte_length : 0;
      const auto overlaps = valid_range && std::ranges::any_of(ranges, [&](const auto& range) {
                              return byte_offset < range.second && range.first < byte_end;
                            });
      if (byte_length != expected_length || uncompressed_length != expected_length ||
          !valid_range || overlaps) {
        cube = {};
        return environment_ktx2_error::invalid_level;
      }
      cube.levels.push_back({.resolution = resolution,
                             .pixels = bytes.subspan(static_cast<std::size_t>(byte_offset),
                                                     static_cast<std::size_t>(byte_length))});
      ranges.emplace_back(byte_offset, byte_end);
      resolution = std::max(resolution / 2U, 1U);
    }
    return environment_ktx2_error::none;
  } catch (const std::bad_alloc&) {
    cube = {};
    return environment_ktx2_error::invalid_level;
  }
}

} // namespace granit::example::model_viewer
