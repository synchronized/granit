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
      make_shader_cache_key({wgsl, "wgsl", "main", "compute", "tint-r1", "vulkan1.3", "--use-ir"});
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  if (encode_shader_asset({wgsl, spirv, reflection, cache_key, 3, 0, 3, "main"}, first) !=
          shader_asset_error::success ||
      encode_shader_asset({wgsl, spirv, reflection, cache_key, 3, 0, 3, "main"}, second) !=
          shader_asset_error::success ||
      first != second)
    return 1;
  shader_asset_view view;
  const shader_cache_key empty_id{};
  if (decode_shader_asset(first, view) != shader_asset_error::success ||
      validate_shader_asset_payloads(view, wgsl, spirv) != shader_asset_error::success ||
      view.reflection_json != reflection || view.cache_key != cache_key ||
      view.content_id == empty_id || view.stage != 3 || view.entry_point != "main" ||
      view.variant_count != 2)
    return 2;
  const auto* webgpu =
      find_shader_asset_variant(view, shader_asset_backend::webgpu, shader_asset_profile::portable);
  const auto* vulkan =
      find_shader_asset_variant(view, shader_asset_backend::vulkan, shader_asset_profile::portable);
  if (webgpu == nullptr || webgpu->code_format != shader_asset_code_format::wgsl ||
      webgpu->required_features != 0 || webgpu->byte_size != wgsl.size() || vulkan == nullptr ||
      vulkan->code_format != shader_asset_code_format::spirv || vulkan->required_features != 0 ||
      vulkan->byte_size != spirv.size())
    return 17;
  std::vector<std::byte> feature_asset;
  if (encode_shader_asset(
          {wgsl, spirv, reflection, cache_key, 3, UINT64_C(1), 3, "main"}, feature_asset) !=
          shader_asset_error::success ||
      decode_shader_asset(feature_asset, view) != shader_asset_error::success ||
      view.variants[0].required_features != UINT64_C(1) ||
      view.variants[1].required_features != UINT64_C(1))
    return 21;
  std::vector<std::byte> webgpu_only;
  if (encode_shader_asset({wgsl, spirv, reflection, cache_key, 2, 0, 3, "main"}, webgpu_only) !=
      shader_asset_error::success)
    return 19;
  shader_asset_view webgpu_view;
  if (decode_shader_asset(webgpu_only, webgpu_view) != shader_asset_error::success ||
      webgpu_view.variant_count != 1 ||
      find_shader_asset_variant(webgpu_view, shader_asset_backend::webgpu,
                                shader_asset_profile::portable) == nullptr ||
      find_shader_asset_variant(webgpu_view, shader_asset_backend::vulkan,
                                shader_asset_profile::portable) != nullptr)
    return 20;
  const auto same_from_other_directory =
      make_shader_cache_key({wgsl, "wgsl", "main", "compute", "tint-r1", "vulkan1.3", "--use-ir"});
  if (same_from_other_directory != cache_key)
    return 9;
  std::vector<std::byte> other_entry;
  if (encode_shader_asset({wgsl, spirv, reflection, cache_key, 3, 0, 3, "other"}, other_entry) !=
          shader_asset_error::success ||
      other_entry == first || encode_shader_asset({wgsl, spirv, reflection, cache_key, 3, 0, 0},
                                                  other_entry) !=
                                shader_asset_error::invalid_argument)
    return 22;
  const std::array changed_contexts{
      make_shader_cache_key({wgsl, "hlsl", "main", "compute", "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({"@compute @workgroup_size(2) fn main() {}\n", "wgsl", "main",
                             "compute", "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "wgsl", "other", "compute", "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "wgsl", "main", "fragment", "tint-r1", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "wgsl", "main", "compute", "tint-r2", "vulkan1.3", "--use-ir"}),
      make_shader_cache_key({wgsl, "wgsl", "main", "compute", "tint-r1", "webgpu", "--use-ir"}),
      make_shader_cache_key(
          {wgsl, "wgsl", "main", "compute", "tint-r1", "vulkan1.3", "--no-use-ir"})};
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
  corrupted[8] = std::byte{5};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::unsupported_schema)
    return 11;
  corrupted = first;
  corrupted[12] = std::byte{0};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::invalid_layout)
    return 12;
  corrupted = first;
  corrupted[112] = std::byte{2};
  if (decode_shader_asset(corrupted, view) != shader_asset_error::digest_mismatch)
    return 18;
  const auto cache_path = std::filesystem::path{argv[1]} / "fixture.granit-shader";
  std::error_code error;
  std::filesystem::remove_all(cache_path.parent_path(), error);
  bool cache_hit = true;
  if (store_shader_asset(cache_path, second, wgsl, spirv, cache_hit) !=
          shader_asset_error::success ||
      cache_hit)
    return 5;
  if (store_shader_asset(cache_path, second, wgsl, spirv, cache_hit) !=
          shader_asset_error::success ||
      !cache_hit)
    return 6;
  auto changed = second;
  changed.back() ^= std::byte{1};
  if (store_shader_asset(cache_path, changed, wgsl, spirv, cache_hit) !=
      shader_asset_error::invalid_argument)
    return 7;
  const auto loaded_size = std::filesystem::file_size(cache_path, error);
  if (error || loaded_size != second.size())
    return 8;
  const auto wgsl_sidecar = std::filesystem::path{cache_path.string() + ".wgsl"};
  const auto spirv_sidecar = std::filesystem::path{cache_path.string() + ".spv"};
  if (std::filesystem::file_size(wgsl_sidecar, error) != wgsl.size() || error ||
      std::filesystem::file_size(spirv_sidecar, error) != spirv.size() || error)
    return 14;
  if (decode_shader_asset(second, view) != shader_asset_error::success)
    return 16;
  auto damaged_spirv = spirv;
  damaged_spirv[0] ^= std::byte{1};
  if (validate_shader_asset_payloads(view, wgsl, damaged_spirv) !=
      shader_asset_error::digest_mismatch)
    return 15;
  std::filesystem::remove_all(cache_path.parent_path(), error);
  return 0;
}
