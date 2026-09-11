// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/shader_asset.h"
#include "assets/shader_library.h"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <vector>

namespace {

void refresh_digest(std::vector<std::byte>& bytes) {
  const auto digest = granit::tools::shader_bytes_sha256_zeroed(bytes, 80, 32);
  std::ranges::copy(digest, bytes.begin() + 80);
}

} // namespace

int main() {
  using namespace granit::tools;
  constexpr std::string_view wgsl = "@compute @workgroup_size(1) fn main() {}\n";
  constexpr std::array spirv{std::byte{3}, std::byte{2}, std::byte{35}, std::byte{7}};
  constexpr std::string_view reflection = "{\"schema\":1}\n";
  const auto wgsl_bytes = std::span{reinterpret_cast<const std::byte*>(wgsl.data()), wgsl.size()};

  std::vector<std::byte> first_manifest;
  std::vector<std::byte> second_manifest;
  const auto first_key =
      make_shader_cache_key({wgsl, "wgsl", "main", "compute", "tint-r1", "vulkan1.3", ""});
  const auto second_key =
      make_shader_cache_key({wgsl, "wgsl", "other", "compute", "tint-r1", "vulkan1.3", ""});
  if (encode_shader_asset({wgsl, spirv, reflection, first_key, GRANIT_SHADER_BACKEND_ALL_BITS, 0,
                           granit::shader_stage::compute, "main"},
                          first_manifest) != shader_asset_error::success ||
      encode_shader_asset({wgsl, spirv, reflection, second_key, GRANIT_SHADER_BACKEND_ALL_BITS, 0,
                           granit::shader_stage::compute, "other"},
                          second_manifest) != shader_asset_error::success) {
    return 1;
  }
  const std::array sources{
      shader_library_asset_source{first_manifest, wgsl_bytes, spirv},
      shader_library_asset_source{second_manifest, wgsl_bytes, spirv},
  };
  const std::array reversed{sources[1], sources[0]};
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  if (encode_shader_library({sources, GRANIT_SHADER_BACKEND_ALL_BITS}, first) !=
          shader_library_error::success ||
      encode_shader_library({reversed, GRANIT_SHADER_BACKEND_ALL_BITS}, second) !=
          shader_library_error::success ||
      first != second) {
    return 2;
  }

  shader_library_view library;
  if (decode_shader_library(first, library) != shader_library_error::success ||
      library.backend_mask != GRANIT_SHADER_BACKEND_ALL_BITS || library.shaders.size() != 2 ||
      library.payloads.size() != 2 || std::ranges::any_of(library.shaders, [](const auto& shader) {
        return shader.variants.size() != 2;
      })) {
    return 3;
  }
  shader_asset_view first_asset;
  if (decode_shader_asset(first_manifest, first_asset) != shader_asset_error::success ||
      find_shader_library_shader(library, first_asset.content_id) == nullptr) {
    return 4;
  }
  auto missing_id = first_asset.content_id;
  missing_id[0] ^= std::byte{1};
  if (find_shader_library_shader(library, missing_id) != nullptr)
    return 5;

  const std::array duplicates{sources[0], sources[0]};
  std::vector<std::byte> deduplicated;
  if (encode_shader_library({duplicates, GRANIT_SHADER_BACKEND_ALL_BITS}, deduplicated) !=
          shader_library_error::success ||
      decode_shader_library(deduplicated, library) != shader_library_error::success ||
      library.shaders.size() != 1 || library.payloads.size() != 2) {
    return 6;
  }

  std::vector<std::byte> vulkan_only;
  if (encode_shader_library({sources, GRANIT_SHADER_BACKEND_VULKAN_BIT}, vulkan_only) !=
          shader_library_error::success ||
      decode_shader_library(vulkan_only, library) != shader_library_error::success ||
      library.backend_mask != GRANIT_SHADER_BACKEND_VULKAN_BIT || library.payloads.size() != 1 ||
      std::ranges::any_of(library.shaders, [](const auto& shader) {
        return shader.variants.size() != 1 ||
               shader.variants.front().backend != shader_asset_backend::vulkan;
      })) {
    return 7;
  }

  auto missing_payload = sources;
  missing_payload[0].spirv = {};
  if (encode_shader_library({missing_payload, GRANIT_SHADER_BACKEND_VULKAN_BIT}, second) !=
      shader_library_error::missing_payload) {
    return 8;
  }
  if (encode_shader_library({sources, 0}, second) != shader_library_error::invalid_argument ||
      encode_shader_library({sources, 4}, second) != shader_library_error::invalid_argument) {
    return 9;
  }

  auto corrupted = first;
  corrupted.back() ^= std::byte{1};
  if (decode_shader_library(corrupted, library) != shader_library_error::digest_mismatch)
    return 10;
  corrupted = first;
  corrupted[0] = std::byte{0};
  if (decode_shader_library(corrupted, library) != shader_library_error::invalid_magic)
    return 11;
  corrupted = first;
  corrupted[8] = std::byte{2};
  if (decode_shader_library(corrupted, library) != shader_library_error::unsupported_schema)
    return 12;
  corrupted = first;
  corrupted[112] = std::byte{1};
  refresh_digest(corrupted);
  if (decode_shader_library(corrupted, library) != shader_library_error::invalid_layout)
    return 13;
  corrupted = first;
  corrupted.back() ^= std::byte{1};
  refresh_digest(corrupted);
  if (decode_shader_library(corrupted, library) != shader_library_error::digest_mismatch)
    return 14;
  return 0;
}
