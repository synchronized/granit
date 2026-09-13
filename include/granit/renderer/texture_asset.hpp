// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_ASSET_HPP_
#define GRANIT_TEXTURE_ASSET_HPP_

#include <granit/core/content_id.hpp>
#include <granit/core/result.hpp>
#include <granit/renderer/texture_asset.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace granit {

using texture_content_id = asset_content_id;

struct texture_asset_info {
  texture_content_id content_id{};
  granit_texture_dimension dimension{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t depth{};
  std::uint32_t array_layers{};
  std::uint32_t mip_levels{};
  std::vector<granit_texture_asset_variant_info> variants;
  std::vector<granit_texture_asset_subresource_info> subresources;
};

struct texture_asset_selection_options {
  granit_texture_usage required_usage{GRANIT_TEXTURE_USAGE_SAMPLED_BIT};
  granit_texture_format_feature_flags required_features{};
};

struct texture_asset_selection {
  std::uint32_t variant_index{UINT32_MAX};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  std::uint64_t payload_offset{};
  std::uint64_t payload_size{};
};

[[nodiscard]] inline result encode_texture_asset(const texture_asset_info& info,
                                                 std::vector<std::byte>& manifest) noexcept {
  if (info.variants.size() > UINT32_MAX || info.subresources.size() > UINT32_MAX)
    return result::invalid_argument;
  granit_texture_asset_info native = GRANIT_TEXTURE_ASSET_INFO_INIT;
  native.schema_version = GRANIT_TEXTURE_ASSET_SCHEMA_VERSION;
  std::memcpy(native.content_id, info.content_id.data(), info.content_id.size());
  native.dimension = info.dimension;
  native.width = info.width;
  native.height = info.height;
  native.depth = info.depth;
  native.array_layers = info.array_layers;
  native.mip_levels = info.mip_levels;
  native.variants = const_cast<granit_texture_asset_variant_info*>(info.variants.data());
  native.variant_count = static_cast<std::uint32_t>(info.variants.size());
  native.variant_capacity = native.variant_count;
  native.subresources =
      const_cast<granit_texture_asset_subresource_info*>(info.subresources.data());
  native.subresource_count = static_cast<std::uint32_t>(info.subresources.size());
  native.subresource_capacity = native.subresource_count;
  std::uint64_t required_size = 0;
  auto value = from_native(granit_texture_asset_encode(&native, nullptr, &required_size));
  if (!value)
    return value;
  try {
    std::vector<std::byte> replacement(static_cast<std::size_t>(required_size));
    value = from_native(granit_texture_asset_encode(&native, replacement.data(), &required_size));
    if (value)
      manifest = std::move(replacement);
    return value;
  } catch (...) {
    return result::out_of_memory;
  }
}

[[nodiscard]] inline result inspect_texture_asset(std::span<const std::byte> manifest,
                                                  texture_asset_info& info) noexcept {
  granit_texture_asset_info native = GRANIT_TEXTURE_ASSET_INFO_INIT;
  auto value = from_native(granit_texture_asset_inspect(manifest.data(), manifest.size(), &native));
  if (!value)
    return value;
  try {
    texture_asset_info replacement;
    replacement.variants.resize(native.variant_count);
    replacement.subresources.resize(native.subresource_count);
    native.variants = replacement.variants.data();
    native.variant_capacity = native.variant_count;
    native.subresources = replacement.subresources.data();
    native.subresource_capacity = native.subresource_count;
    value = from_native(granit_texture_asset_inspect(manifest.data(), manifest.size(), &native));
    if (!value)
      return value;
    std::memcpy(replacement.content_id.data(), native.content_id, replacement.content_id.size());
    replacement.dimension = native.dimension;
    replacement.width = native.width;
    replacement.height = native.height;
    replacement.depth = native.depth;
    replacement.array_layers = native.array_layers;
    replacement.mip_levels = native.mip_levels;
    info = std::move(replacement);
    return result::success;
  } catch (...) {
    return result::out_of_memory;
  }
}

[[nodiscard]] inline result
select_texture_asset_variant(granit_renderer renderer, std::span<const std::byte> manifest,
                             texture_asset_selection& selection,
                             texture_asset_selection_options options = {}) noexcept {
  const granit_texture_asset_selection_desc desc{GRANIT_TEXTURE_ASSET_SELECTION_DESC_SIZE,
                                                 options.required_usage, options.required_features,
                                                 UINT32_C(0)};
  granit_texture_asset_selection native = GRANIT_TEXTURE_ASSET_SELECTION_INIT;
  const auto value = from_native(granit_renderer_select_texture_asset_variant(
      renderer, manifest.data(), manifest.size(), &desc, &native));
  if (value)
    selection = {native.variant_index, native.format, native.payload_offset, native.payload_size};
  return value;
}

[[nodiscard]] inline result
write_texture_asset_mips(granit_renderer renderer, granit_upload_batch batch,
                         granit_texture texture, std::span<const std::byte> manifest,
                         std::span<const std::byte> payload, std::uint32_t variant_index,
                         std::uint32_t first_mip, std::uint32_t mip_count) noexcept {
  return from_native(granit_upload_batch_write_texture_asset_mips(
      renderer, batch, texture, manifest.data(), manifest.size(), payload.data(), payload.size(),
      variant_index, first_mip, mip_count));
}

} // namespace granit

#endif
