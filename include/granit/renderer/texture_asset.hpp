// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_ASSET_HPP_
#define GRANIT_TEXTURE_ASSET_HPP_

#include <granit/core/content_id.hpp>
#include <granit/core/result.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/texture_asset.h>
#include <granit/renderer/upload_batch.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace granit {

using texture_content_id = asset_content_id;

struct texture_asset_variant_info {
  texture_format format{texture_format::undefined};
  texture_usage usage{};
  std::uint32_t first_subresource{};
  std::uint32_t subresource_count{};
  std::uint64_t payload_offset{};
  std::uint64_t payload_size{};
  content_digest payload_digest{};
};

struct texture_asset_subresource_info {
  std::uint32_t mip_level{};
  std::uint32_t array_layer{};
  std::uint64_t data_offset{};
  std::uint64_t data_size{};
  std::uint32_t bytes_per_row{};
  std::uint32_t rows_per_image{};
};

struct texture_asset_info {
  texture_content_id content_id{};
  texture_dimension dimension{texture_dimension::two_dimensional};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t depth{};
  std::uint32_t array_layers{};
  std::uint32_t mip_levels{};
  std::vector<texture_asset_variant_info> variants;
  std::vector<texture_asset_subresource_info> subresources;
};

struct texture_asset_selection_options {
  texture_usage required_usage{texture_usage::sampled};
  texture_format_feature required_features{};
};

struct texture_asset_selection {
  std::uint32_t variant_index{UINT32_MAX};
  texture_format format{texture_format::undefined};
  std::uint64_t payload_offset{};
  std::uint64_t payload_size{};
};

[[nodiscard]] inline result inspect_texture_asset(std::span<const std::byte> manifest,
                                                  texture_asset_info& info) noexcept {
  granit_texture_asset_info native = GRANIT_TEXTURE_ASSET_INFO_INIT;
  auto value = from_native(granit_texture_asset_inspect(manifest.data(), manifest.size(), &native));
  if (!value)
    return value;
  try {
    texture_asset_info replacement;
    std::vector<granit_texture_asset_variant_info> native_variants(native.variant_count);
    std::vector<granit_texture_asset_subresource_info> native_subresources(
        native.subresource_count);
    native.variants = native_variants.data();
    native.variant_capacity = native.variant_count;
    native.subresources = native_subresources.data();
    native.subresource_capacity = native.subresource_count;
    value = from_native(granit_texture_asset_inspect(manifest.data(), manifest.size(), &native));
    if (!value)
      return value;
    std::memcpy(replacement.content_id.data(), native.content_id, replacement.content_id.size());
    replacement.dimension = static_cast<texture_dimension>(native.dimension);
    replacement.width = native.width;
    replacement.height = native.height;
    replacement.depth = native.depth;
    replacement.array_layers = native.array_layers;
    replacement.mip_levels = native.mip_levels;
    replacement.variants.reserve(native_variants.size());
    for (const auto& variant : native_variants) {
      texture_asset_variant_info converted{
          .format = static_cast<texture_format>(variant.format),
          .usage = static_cast<texture_usage>(variant.usage),
          .first_subresource = variant.first_subresource,
          .subresource_count = variant.subresource_count,
          .payload_offset = variant.payload_offset,
          .payload_size = variant.payload_size,
      };
      std::memcpy(converted.payload_digest.data(), variant.payload_digest,
                  converted.payload_digest.size());
      replacement.variants.push_back(converted);
    }
    replacement.subresources.reserve(native_subresources.size());
    for (const auto& subresource : native_subresources) {
      replacement.subresources.push_back({.mip_level = subresource.mip_level,
                                          .array_layer = subresource.array_layer,
                                          .data_offset = subresource.data_offset,
                                          .data_size = subresource.data_size,
                                          .bytes_per_row = subresource.bytes_per_row,
                                          .rows_per_image = subresource.rows_per_image});
    }
    info = std::move(replacement);
    return result::success;
  } catch (const std::bad_alloc&) {
    return result::out_of_memory;
  } catch (...) {
    return result::internal;
  }
}

[[nodiscard]] inline result
select_texture_asset_variant(renderer_ref owner, std::span<const std::byte> manifest,
                             texture_asset_selection& selection,
                             texture_asset_selection_options options = {}) noexcept {
  const auto renderer = owner.native_handle();
  const granit_texture_asset_selection_desc desc{
      GRANIT_TEXTURE_ASSET_SELECTION_DESC_SIZE,
      static_cast<granit_texture_usage>(options.required_usage),
      static_cast<granit_texture_format_feature_flags>(options.required_features), UINT32_C(0)};
  granit_texture_asset_selection native = GRANIT_TEXTURE_ASSET_SELECTION_INIT;
  const auto value = from_native(granit_renderer_select_texture_asset_variant(
      renderer, manifest.data(), manifest.size(), &desc, &native));
  if (value)
    selection = {native.variant_index, static_cast<texture_format>(native.format),
                 native.payload_offset, native.payload_size};
  return value;
}

[[nodiscard]] inline result
select_texture_asset_variant(renderer& owner, std::span<const std::byte> manifest,
                             texture_asset_selection& selection,
                             texture_asset_selection_options options = {}) noexcept {
  return select_texture_asset_variant(owner.ref(), manifest, selection, options);
}

[[nodiscard]] inline result
write_texture_asset_mips(upload_batch& batch, texture_ref texture,
                         std::span<const std::byte> manifest,
                         std::span<const std::byte> payload, std::uint32_t variant_index,
                         std::uint32_t first_mip, std::uint32_t mip_count) noexcept {
  return from_native(granit_upload_batch_write_texture_asset_mips(
      batch.owner().native_handle(), batch.native_handle(), texture.native_handle(), manifest.data(),
      manifest.size(), payload.data(), payload.size(), variant_index, first_mip, mip_count));
}

} // namespace granit

#endif
