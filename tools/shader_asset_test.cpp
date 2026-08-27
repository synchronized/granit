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
  const auto cache_key =
      make_shader_cache_key({wgsl, "main", "compute", "tint-r1", "vulkan1.3", "--use-ir"});
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  if (encode_shader_asset({wgsl, spirv, reflection, cache_key}, first) !=
          shader_asset_error::success ||
      encode_shader_asset({wgsl, spirv, reflection, cache_key}, second) !=
          shader_asset_error::success ||
      first != second)
    return 1;
  shader_asset_view view;
  if (decode_shader_asset(first, view) != shader_asset_error::success || view.wgsl != wgsl ||
      !std::ranges::equal(view.spirv, spirv) || view.reflection_json != reflection ||
      view.cache_key != cache_key)
    return 2;
  const auto same_from_other_directory =
      make_shader_cache_key({wgsl, "main", "compute", "tint-r1", "vulkan1.3", "--use-ir"});
  if (same_from_other_directory != cache_key)
    return 9;
  const std::array changed_contexts{
      make_shader_cache_key({"@compute @workgroup_size(2) fn main() {}\n", "main", "compute",
                             "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "other", "compute", "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "main", "fragment", "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "main", "compute", "tint-r2", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "main", "compute", "tint-r1", "webgpu", "--use-ir"}),
      make_shader_cache_key({wgsl, "main", "compute", "tint-r1", "vulkan1.3", "--no-use-ir"})};
  if (std::ranges::any_of(changed_contexts,
                          [&](const auto& changed_key) { return changed_key == cache_key; }))
    return 13;
  auto corrupted = first;
  corrupted.back() ^= std::byte{1};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::digest_mismatch)
    return 3;
  corrupted = first;
  corrupted[0] = std::byte{0};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::invalid_magic)
    return 4;
  corrupted = first;
  corrupted[8] = std::byte{3};
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
