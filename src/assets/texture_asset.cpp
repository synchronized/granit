// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "texture_asset.h"

#include "core/texture_format.h"
#include "shader_format/digest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace granit::detail {
namespace {

constexpr std::array magic{std::byte{'G'}, std::byte{'R'}, std::byte{'N'}, std::byte{'T'},
                           std::byte{'E'}, std::byte{'X'}, std::byte{'A'}, std::byte{0}};
constexpr uint32_t schema_version = 1;
constexpr uint32_t header_size = 80;
constexpr uint32_t variant_size = 72;
constexpr uint32_t subresource_size = 40;
constexpr uint32_t maximum_variant_count = 32;
constexpr uint32_t maximum_subresource_count = 65536;

uint32_t read_u32(std::span<const std::byte> bytes, size_t offset) noexcept {
  uint32_t value = 0;
  for (uint32_t index = 0; index < 4; ++index)
    value |= static_cast<uint32_t>(std::to_integer<unsigned char>(bytes[offset + index]))
             << (index * 8U);
  return value;
}

uint64_t read_u64(std::span<const std::byte> bytes, size_t offset) noexcept {
  uint64_t value = 0;
  for (uint32_t index = 0; index < 8; ++index)
    value |= static_cast<uint64_t>(std::to_integer<unsigned char>(bytes[offset + index]))
             << (index * 8U);
  return value;
}

void write_u32(std::span<std::byte> bytes, size_t offset, uint32_t value) noexcept {
  for (uint32_t index = 0; index < 4; ++index)
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

void write_u64(std::span<std::byte> bytes, size_t offset, uint64_t value) noexcept {
  for (uint32_t index = 0; index < 8; ++index)
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

bool checked_table_size(uint32_t variant_count, uint32_t subresource_count,
                        uint64_t& size) noexcept {
  const uint64_t variants = uint64_t{variant_count} * variant_size;
  const uint64_t subresources = uint64_t{subresource_count} * subresource_size;
  if (variants > UINT64_MAX - header_size || subresources > UINT64_MAX - header_size - variants)
    return false;
  size = header_size + variants + subresources;
  return true;
}

bool valid_dimension(granit_texture_dimension dimension, uint32_t depth,
                     uint32_t array_layers) noexcept {
  switch (dimension) {
  case GRANIT_TEXTURE_DIMENSION_1D:
    return depth == 1;
  case GRANIT_TEXTURE_DIMENSION_2D:
    return depth == 1;
  case GRANIT_TEXTURE_DIMENSION_3D:
    return array_layers == 1;
  case GRANIT_TEXTURE_DIMENSION_CUBE:
    return depth == 1 && array_layers % 6 == 0;
  default:
    return false;
  }
}

uint32_t mip_extent(uint32_t extent, uint32_t mip) noexcept {
  return std::max(UINT32_C(1), extent >> std::min(mip, UINT32_C(31)));
}

bool valid_subresource(const texture_asset_view& asset,
                       const granit_texture_asset_variant_info& variant,
                       const granit_texture_asset_subresource_info& subresource) noexcept {
  if (subresource.reserved[0] != 0 || subresource.reserved[1] != 0 ||
      subresource.mip_level >= asset.mip_levels || subresource.array_layer >= asset.array_layers ||
      subresource.data_offset > variant.payload_size ||
      subresource.data_size > variant.payload_size - subresource.data_offset)
    return false;
  granit_texture_data_layout layout{subresource.data_offset, subresource.bytes_per_row,
                                    subresource.rows_per_image};
  texture_transfer_footprint footprint{};
  const auto depth = asset.dimension == GRANIT_TEXTURE_DIMENSION_3D
                         ? mip_extent(asset.depth, subresource.mip_level)
                         : UINT32_C(1);
  if (!calculate_texture_transfer_footprint(
          variant.format, mip_extent(asset.width, subresource.mip_level),
          mip_extent(asset.height, subresource.mip_level), depth, layout, footprint))
    return false;
  return footprint.required_size <= subresource.data_size;
}

} // namespace

texture_asset_error encode_texture_asset(const texture_asset_view& asset,
                                         std::vector<std::byte>& output) {
  if (asset.variants.empty() || asset.variants.size() > maximum_variant_count ||
      asset.subresources.empty() || asset.subresources.size() > maximum_subresource_count)
    return texture_asset_error::invalid_argument;
  uint64_t encoded_size = 0;
  if (!checked_table_size(static_cast<uint32_t>(asset.variants.size()),
                          static_cast<uint32_t>(asset.subresources.size()), encoded_size) ||
      encoded_size > std::numeric_limits<size_t>::max())
    return texture_asset_error::invalid_argument;
  std::vector<std::byte> encoded(static_cast<size_t>(encoded_size));
  std::ranges::copy(magic, encoded.begin());
  write_u32(encoded, 8, schema_version);
  write_u32(encoded, 12, header_size);
  write_u32(encoded, 16, asset.width);
  write_u32(encoded, 20, asset.height);
  write_u32(encoded, 24, asset.depth);
  write_u32(encoded, 28, asset.array_layers);
  write_u32(encoded, 32, asset.mip_levels);
  write_u32(encoded, 36, asset.dimension);
  write_u32(encoded, 40, static_cast<uint32_t>(asset.variants.size()));
  write_u32(encoded, 44, static_cast<uint32_t>(asset.subresources.size()));
  std::ranges::copy(asset.content_id, encoded.begin() + 48);
  for (size_t index = 0; index < asset.variants.size(); ++index) {
    const auto offset = header_size + index * variant_size;
    const auto& variant = asset.variants[index];
    write_u32(encoded, offset, variant.format);
    write_u32(encoded, offset + 4, variant.usage);
    write_u32(encoded, offset + 8, variant.first_subresource);
    write_u32(encoded, offset + 12, variant.subresource_count);
    write_u64(encoded, offset + 16, variant.payload_offset);
    write_u64(encoded, offset + 24, variant.payload_size);
    std::memcpy(encoded.data() + offset + 32, variant.payload_digest, GRANIT_TEXTURE_ASSET_ID_SIZE);
    write_u32(encoded, offset + 64, variant.reserved[0]);
    write_u32(encoded, offset + 68, variant.reserved[1]);
  }
  const auto table = header_size + asset.variants.size() * variant_size;
  for (size_t index = 0; index < asset.subresources.size(); ++index) {
    const auto offset = table + index * subresource_size;
    const auto& subresource = asset.subresources[index];
    write_u32(encoded, offset, subresource.mip_level);
    write_u32(encoded, offset + 4, subresource.array_layer);
    write_u64(encoded, offset + 8, subresource.data_offset);
    write_u64(encoded, offset + 16, subresource.data_size);
    write_u32(encoded, offset + 24, subresource.bytes_per_row);
    write_u32(encoded, offset + 28, subresource.rows_per_image);
    write_u32(encoded, offset + 32, subresource.reserved[0]);
    write_u32(encoded, offset + 36, subresource.reserved[1]);
  }
  texture_asset_view validated;
  if (decode_texture_asset(encoded, validated) != texture_asset_error::success)
    return texture_asset_error::invalid_layout;
  output = std::move(encoded);
  return texture_asset_error::success;
}

texture_asset_error decode_texture_asset(std::span<const std::byte> manifest,
                                         texture_asset_view& output) {
  if (manifest.size() < header_size)
    return texture_asset_error::invalid_argument;
  if (!std::ranges::equal(magic, manifest.first(magic.size())))
    return texture_asset_error::invalid_argument;
  if (read_u32(manifest, 8) != schema_version)
    return texture_asset_error::unsupported_schema;
  if (read_u32(manifest, 12) != header_size)
    return texture_asset_error::invalid_layout;

  texture_asset_view result;
  result.width = read_u32(manifest, 16);
  result.height = read_u32(manifest, 20);
  result.depth = read_u32(manifest, 24);
  result.array_layers = read_u32(manifest, 28);
  result.mip_levels = read_u32(manifest, 32);
  result.dimension = read_u32(manifest, 36);
  const auto variant_count = read_u32(manifest, 40);
  const auto subresource_count = read_u32(manifest, 44);
  std::ranges::copy(manifest.subspan(48, result.content_id.size()), result.content_id.begin());
  const bool empty_id =
      std::ranges::all_of(result.content_id, [](std::byte value) { return value == std::byte{0}; });
  uint64_t expected_size = 0;
  if (result.width == 0 || result.height == 0 || result.depth == 0 || result.array_layers == 0 ||
      result.mip_levels == 0 || empty_id || variant_count == 0 ||
      variant_count > maximum_variant_count || subresource_count == 0 ||
      subresource_count > maximum_subresource_count ||
      !valid_dimension(result.dimension, result.depth, result.array_layers) ||
      !checked_table_size(variant_count, subresource_count, expected_size) ||
      expected_size != manifest.size())
    return texture_asset_error::invalid_layout;

  result.variants.resize(variant_count);
  result.subresources.resize(subresource_count);
  uint32_t expected_first = 0;
  const auto subresource_table =
      static_cast<size_t>(header_size + uint64_t{variant_count} * variant_size);
  for (uint32_t index = 0; index < variant_count; ++index) {
    const auto offset = static_cast<size_t>(header_size + uint64_t{index} * variant_size);
    auto& variant = result.variants[index];
    variant.format = read_u32(manifest, offset);
    variant.usage = read_u32(manifest, offset + 4);
    variant.first_subresource = read_u32(manifest, offset + 8);
    variant.subresource_count = read_u32(manifest, offset + 12);
    variant.payload_offset = read_u64(manifest, offset + 16);
    variant.payload_size = read_u64(manifest, offset + 24);
    std::memcpy(variant.payload_digest, manifest.data() + offset + 32,
                GRANIT_TEXTURE_ASSET_ID_SIZE);
    variant.reserved[0] = read_u32(manifest, offset + 64);
    variant.reserved[1] = read_u32(manifest, offset + 68);
    const uint64_t expected_subresources = uint64_t{result.mip_levels} * result.array_layers;
    if (texture_format_block(variant.format).bytes == 0 || variant.usage == 0 ||
        variant.payload_size == 0 || variant.reserved[0] != 0 || variant.reserved[1] != 0 ||
        variant.first_subresource != expected_first ||
        variant.subresource_count != expected_subresources ||
        variant.subresource_count > subresource_count - expected_first)
      return texture_asset_error::invalid_layout;
    expected_first += variant.subresource_count;
  }
  if (expected_first != subresource_count)
    return texture_asset_error::invalid_layout;

  for (uint32_t index = 0; index < subresource_count; ++index) {
    const auto offset = subresource_table + static_cast<size_t>(index) * subresource_size;
    auto& subresource = result.subresources[index];
    subresource.mip_level = read_u32(manifest, offset);
    subresource.array_layer = read_u32(manifest, offset + 4);
    subresource.data_offset = read_u64(manifest, offset + 8);
    subresource.data_size = read_u64(manifest, offset + 16);
    subresource.bytes_per_row = read_u32(manifest, offset + 24);
    subresource.rows_per_image = read_u32(manifest, offset + 28);
    subresource.reserved[0] = read_u32(manifest, offset + 32);
    subresource.reserved[1] = read_u32(manifest, offset + 36);
  }
  for (const auto& variant : result.variants) {
    std::vector<bool> present(static_cast<size_t>(result.mip_levels) * result.array_layers);
    for (uint32_t index = 0; index < variant.subresource_count; ++index) {
      const auto& subresource = result.subresources[variant.first_subresource + index];
      const auto key =
          static_cast<size_t>(subresource.array_layer) * result.mip_levels + subresource.mip_level;
      if (key >= present.size() || present[key] || !valid_subresource(result, variant, subresource))
        return texture_asset_error::invalid_layout;
      present[key] = true;
      for (uint32_t prior_index = 0; prior_index < index; ++prior_index) {
        const auto& prior = result.subresources[variant.first_subresource + prior_index];
        const auto prior_end = prior.data_offset + prior.data_size;
        const auto current_end = subresource.data_offset + subresource.data_size;
        if (subresource.data_offset < prior_end && prior.data_offset < current_end)
          return texture_asset_error::invalid_layout;
      }
    }
  }
  output = std::move(result);
  return texture_asset_error::success;
}

bool validate_texture_asset_payload(const texture_asset_view& asset, uint32_t variant_index,
                                    std::span<const std::byte> payload) noexcept {
  if (variant_index >= asset.variants.size())
    return false;
  const auto& variant = asset.variants[variant_index];
  if (variant.payload_offset > payload.size() ||
      variant.payload_size > payload.size() - variant.payload_offset)
    return false;
  const auto bytes = payload.subspan(static_cast<size_t>(variant.payload_offset),
                                     static_cast<size_t>(variant.payload_size));
  const auto actual = shader_format::shader_bytes_sha256(bytes);
  return std::memcmp(actual.data(), variant.payload_digest, actual.size()) == 0;
}

} // namespace granit::detail
