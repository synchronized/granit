// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_asset.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2)
    return 10;
  using namespace granit::tools;
  constexpr std::string_view wgsl = "@compute @workgroup_size(1) fn main() {}\n";
  constexpr std::array spirv{std::byte{3}, std::byte{2}, std::byte{35}, std::byte{7}};
  constexpr std::string_view reflection = "{\"schema\":1}\n";
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  if (encode_shader_asset({wgsl, spirv, reflection}, first) != shader_asset_error::success ||
      encode_shader_asset({wgsl, spirv, reflection}, second) != shader_asset_error::success ||
      first != second)
    return 1;
  constexpr std::array expected_digest{
      std::byte{0xba}, std::byte{0x66}, std::byte{0x15}, std::byte{0x37}, std::byte{0x5c},
      std::byte{0xd4}, std::byte{0x97}, std::byte{0x11}, std::byte{0xa7}, std::byte{0x68},
      std::byte{0xe3}, std::byte{0x26}, std::byte{0x00}, std::byte{0xe7}, std::byte{0x9e},
      std::byte{0xf1}, std::byte{0xd4}, std::byte{0x34}, std::byte{0xfe}, std::byte{0xfb},
      std::byte{0x62}, std::byte{0xb4}, std::byte{0xc8}, std::byte{0xa7}, std::byte{0xdc},
      std::byte{0xdb}, std::byte{0xb9}, std::byte{0xb8}, std::byte{0xd6}, std::byte{0xd6},
      std::byte{0x47}, std::byte{0xa3}};
  if (!std::ranges::equal(expected_digest,
                          std::span<const std::byte>{first}.subspan(48, expected_digest.size())))
    return 9;
  shader_asset_view view;
  if (decode_shader_asset(first, view) != shader_asset_error::success || view.wgsl != wgsl ||
      !std::ranges::equal(view.spirv, spirv) || view.reflection_json != reflection)
    return 2;
  auto corrupted = first;
  corrupted.back() ^= std::byte{1};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::digest_mismatch)
    return 3;
  corrupted = first;
  corrupted[0] = std::byte{0};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::invalid_magic)
    return 4;
  corrupted = first;
  corrupted[8] = std::byte{2};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::unsupported_schema)
    return 11;
  corrupted = first;
  corrupted[12] = std::byte{0};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::invalid_layout)
    return 12;
  const auto cache_path = std::filesystem::path{argv[1]} / "fixture.granit-shader";
  std::error_code error;
  std::filesystem::remove_all(cache_path.parent_path(), error);
  bool cache_hit = true;
  if (store_shader_asset(cache_path, second, cache_hit) != shader_asset_error::success || cache_hit)
    return 5;
  if (store_shader_asset(cache_path, second, cache_hit) != shader_asset_error::success ||
      !cache_hit)
    return 6;
  auto changed = second;
  changed.back() ^= std::byte{1};
  if (store_shader_asset(cache_path, changed, cache_hit) != shader_asset_error::invalid_argument)
    return 7;
  const auto loaded_size = std::filesystem::file_size(cache_path, error);
  if (error || loaded_size != second.size())
    return 8;
  std::filesystem::remove_all(cache_path.parent_path(), error);
  return 0;
}
