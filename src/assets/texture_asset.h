// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSETS_TEXTURE_ASSET_H_
#define GRANIT_ASSETS_TEXTURE_ASSET_H_

#include <granit/renderer/texture_asset.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace granit::detail {

enum class texture_asset_error { success, invalid_argument, unsupported_schema, invalid_layout };

struct texture_asset_view {
  std::array<std::byte, GRANIT_TEXTURE_ASSET_ID_SIZE> content_id{};
  granit_texture_dimension dimension{};
  uint32_t width{};
  uint32_t height{};
  uint32_t depth{};
  uint32_t array_layers{};
  uint32_t mip_levels{};
  std::vector<granit_texture_asset_variant_info> variants;
  std::vector<granit_texture_asset_subresource_info> subresources;
};

texture_asset_error decode_texture_asset(std::span<const std::byte> manifest,
                                         texture_asset_view& output);
texture_asset_error encode_texture_asset(const texture_asset_view& asset,
                                         std::vector<std::byte>& output);
bool validate_texture_asset_payload(const texture_asset_view& asset, uint32_t variant_index,
                                    std::span<const std::byte> payload) noexcept;

} // namespace granit::detail

#endif
