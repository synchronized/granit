// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_formats/shader/shader_library.h"
#include "asset_formats/shader/shader_object.h"
#include "core/sha256.h"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <vector>

namespace {

void refresh_digest(std::vector<std::byte>& bytes) {
  const auto digest = granit::detail::sha256_bytes_with_zeroed_range(bytes, 80, 32);
  std::ranges::copy(digest, bytes.begin() + 80);
}

} // namespace

int main() {
  using namespace granit::detail::shader_format;
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
  if (encode_shader_object({wgsl, spirv, reflection, first_key, GRANIT_SHADER_BACKEND_ALL_BITS, 0,
                            granit::shader_stage::compute, "main"},
                           first_manifest) != shader_object_error::success ||
      encode_shader_object({wgsl, spirv, reflection, second_key, GRANIT_SHADER_BACKEND_ALL_BITS, 0,
                            granit::shader_stage::compute, "other"},
                           second_manifest) != shader_object_error::success) {
    return 1;
  }
  const std::array sources{
      shader_library_object_source{first_manifest, wgsl_bytes, spirv, "main.compute"},
      shader_library_object_source{second_manifest, wgsl_bytes, spirv, "other.compute"},
  };
  const std::array reversed{sources[1], sources[0]};
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  if (encode_shader_library({sources, GRANIT_SHADER_BACKEND_ALL_BITS, "codec"}, first) !=
          shader_library_error::success ||
      encode_shader_library({reversed, GRANIT_SHADER_BACKEND_ALL_BITS, "codec"}, second) !=
          shader_library_error::success ||
      first != second) {
    return 2;
  }

  shader_library_view library;
  if (decode_shader_library(first, library) != shader_library_error::success ||
      library.backend_mask != GRANIT_SHADER_BACKEND_ALL_BITS || library.shaders.size() != 2 ||
      library.name != "codec" || library.names.size() != 2 || library.payloads.size() != 2 ||
      std::ranges::any_of(library.shaders,
                          [](const auto& shader) { return shader.variants.size() != 2; })) {
    return 3;
  }
  shader_object_view first_asset;
  if (decode_shader_object(first_manifest, first_asset) != shader_object_error::success ||
      find_shader_library_shader(library, first_asset.content_id) == nullptr) {
    return 4;
  }
  if (find_shader_library_shader(library, "main.compute") == nullptr ||
      find_shader_library_shader(library, "missing.compute") != nullptr)
    return 5;
  auto missing_id = first_asset.content_id;
  missing_id[0] ^= std::byte{1};
  if (find_shader_library_shader(library, missing_id) != nullptr)
    return 5;

  const std::array duplicates{sources[0], sources[0]};
  std::vector<std::byte> deduplicated;
  if (encode_shader_library({duplicates, GRANIT_SHADER_BACKEND_ALL_BITS, "codec"}, deduplicated) !=
          shader_library_error::success ||
      decode_shader_library(deduplicated, library) != shader_library_error::success ||
      library.shaders.size() != 1 || library.names.size() != 1 || library.payloads.size() != 2) {
    return 6;
  }

  auto alias = sources[0];
  alias.logical_name = "alias.compute";
  const std::array aliases{sources[0], alias};
  if (encode_shader_library({aliases, GRANIT_SHADER_BACKEND_ALL_BITS, "codec"}, deduplicated) !=
          shader_library_error::success ||
      decode_shader_library(deduplicated, library) != shader_library_error::success ||
      library.shaders.size() != 1 || library.names.size() != 2 ||
      find_shader_library_shader(library, "main.compute") !=
          find_shader_library_shader(library, "alias.compute")) {
    return 7;
  }

  auto conflicting_name = sources[1];
  conflicting_name.logical_name = sources[0].logical_name;
  const std::array conflict{sources[0], conflicting_name};
  if (encode_shader_library({conflict, GRANIT_SHADER_BACKEND_ALL_BITS, "codec"}, second) !=
      shader_library_error::conflicting_shader) {
    return 8;
  }

  std::vector<std::byte> vulkan_only;
  if (encode_shader_library({sources, GRANIT_SHADER_BACKEND_VULKAN_BIT, "codec"}, vulkan_only) !=
          shader_library_error::success ||
      decode_shader_library(vulkan_only, library) != shader_library_error::success ||
      library.backend_mask != GRANIT_SHADER_BACKEND_VULKAN_BIT || library.payloads.size() != 1 ||
      std::ranges::any_of(library.shaders, [](const auto& shader) {
        return shader.variants.size() != 1 ||
               shader.variants.front().backend != shader_object_backend::vulkan;
      })) {
    return 9;
  }

  auto missing_payload = sources;
  missing_payload[0].spirv = {};
  if (encode_shader_library({missing_payload, GRANIT_SHADER_BACKEND_VULKAN_BIT, "codec"}, second) !=
      shader_library_error::missing_payload) {
    return 10;
  }
  if (encode_shader_library({sources, 0, "codec"}, second) !=
          shader_library_error::invalid_argument ||
      encode_shader_library({sources, 4, "codec"}, second) !=
          shader_library_error::invalid_argument) {
    return 11;
  }

  auto corrupted = first;
  corrupted.back() ^= std::byte{1};
  if (decode_shader_library(corrupted, library) != shader_library_error::digest_mismatch)
    return 12;
  corrupted = first;
  corrupted[0] = std::byte{0};
  if (decode_shader_library(corrupted, library) != shader_library_error::invalid_magic)
    return 13;
  corrupted = first;
  corrupted[8] = std::byte{3};
  if (decode_shader_library(corrupted, library) != shader_library_error::unsupported_schema)
    return 14;
  corrupted = first;
  corrupted[140] = std::byte{1};
  refresh_digest(corrupted);
  if (decode_shader_library(corrupted, library) != shader_library_error::invalid_layout)
    return 15;
  corrupted = first;
  corrupted.back() ^= std::byte{1};
  refresh_digest(corrupted);
  if (decode_shader_library(corrupted, library) != shader_library_error::digest_mismatch)
    return 16;
  return 0;
}
