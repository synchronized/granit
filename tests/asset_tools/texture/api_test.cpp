// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/texture_builder.hpp>

#include <algorithm>
#include <array>
#include <string_view>

int main() {
  constexpr std::array<std::byte, 64> payload{};
  const granit_texture_asset_subresource_info subresources[]{{0, 0, 0, 64, 16, 4, {0, 0}}};
  const granit::asset_tools::texture::variant_desc variants[]{
      {GRANIT_TEXTURE_FORMAT_RGBA8_SRGB,
       GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT, payload,
       subresources}};
  const granit::asset_tools::texture::build_desc desc{
      GRANIT_TEXTURE_DIMENSION_2D, 4, 4, 1, 1, 1, variants};
  auto [first_status, first] = granit::asset_tools::texture::build(desc);
  auto [second_status, second] = granit::asset_tools::texture::build(desc);
  if (first_status.failed() || second_status.failed() || !first || !second ||
      first.manifest().size() != 192 || first.payload().size() != payload.size() ||
      !std::ranges::equal(first.manifest(), second.manifest()) ||
      !std::ranges::equal(first.payload(), payload) ||
      first.debug_json().find("\"content_id\"") == std::string_view::npos)
    return 1;
  auto [inspect_status, inspected] = granit::asset_tools::texture::inspect(first.manifest());
  if (inspect_status.failed() || !inspected || !inspected.payload().empty() ||
      inspected.debug_json() != first.debug_json())
    return 2;
  const granit_texture_asset_subresource_info invalid_subresources[]{{0, 0, 0, 63, 16, 4, {0, 0}}};
  const granit::asset_tools::texture::variant_desc invalid_variants[]{
      {GRANIT_TEXTURE_FORMAT_RGBA8_SRGB,
       GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT, payload,
       invalid_subresources}};
  auto invalid_desc = desc;
  invalid_desc.variants = invalid_variants;
  auto [invalid_status, failed] = granit::asset_tools::texture::build(invalid_desc);
  if (invalid_status != granit::result::invalid_argument || !failed || failed.diagnostic().empty())
    return 3;
  return 0;
}
